#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>

#include "cisv/parser.h"
#include "cisv/writer.h"

/**
 * SECURITY: Safe integer parsing with overflow protection.
 * Returns 0 on success, -1 on error (overflow, invalid input, negative when not allowed).
 */
static int safe_parse_int(const char *str, int *result, int allow_negative) {
    if (!str || !result || *str == '\0') {
        return -1;
    }

    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    // Check for conversion errors
    if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        fprintf(stderr, "Error: Integer overflow in argument '%s'\n", str);
        return -1;
    }

    // Check for invalid characters
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid integer '%s'\n", str);
        return -1;
    }

    // Check for negative when not allowed
    if (!allow_negative && val < 0) {
        fprintf(stderr, "Error: Negative value not allowed '%s'\n", str);
        return -1;
    }

    *result = (int)val;
    return 0;
}

/**
 * SECURITY: Safe long parsing with overflow protection.
 */
static int safe_parse_long(const char *str, long *result, int allow_negative) {
    if (!str || !result || *str == '\0') {
        return -1;
    }

    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    // Check for conversion errors
    if (errno == ERANGE) {
        fprintf(stderr, "Error: Integer overflow in argument '%s'\n", str);
        return -1;
    }

    // Check for invalid characters
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid integer '%s'\n", str);
        return -1;
    }

    // Check for negative when not allowed
    if (!allow_negative && val < 0) {
        fprintf(stderr, "Error: Negative value not allowed '%s'\n", str);
        return -1;
    }

    *result = val;
    return 0;
}

typedef struct {
    size_t row_count;
    size_t field_count;
    int count_only;
    int head;
    int tail;
    int *select_cols;
    int select_count;
    FILE *output;

    char ***tail_buffer;
    size_t *tail_field_counts;
    size_t tail_pos;

    char **current_row;
    size_t current_field_count;
    size_t current_field_capacity;
    size_t current_row_num;
    int in_header;

    cisv_config *config;
} cli_context;

static void field_callback(void *user, const char *data, size_t len) {
    cli_context *ctx = (cli_context *)user;

    if (ctx->current_field_count >= ctx->current_field_capacity) {
        size_t new_capacity = ctx->current_field_capacity * 2;
        if (new_capacity < 16) new_capacity = 16;

        char **new_row = realloc(ctx->current_row, new_capacity * sizeof(char *));
        if (!new_row) {
            fprintf(stderr, "Failed to allocate memory for fields\n");
            exit(1);
        }
        ctx->current_row = new_row;
        ctx->current_field_capacity = new_capacity;
    }

    ctx->current_row[ctx->current_field_count] = malloc(len + 1);
    if (!ctx->current_row[ctx->current_field_count]) {
        fprintf(stderr, "Failed to allocate memory for field data\n");
        exit(1);
    }

    memcpy(ctx->current_row[ctx->current_field_count], data, len);
    ctx->current_row[ctx->current_field_count][len] = '\0';
    ctx->current_field_count++;
}

static void row_callback(void *user) {
    cli_context *ctx = (cli_context *)user;

    if (ctx->head > 0 && ctx->current_row_num >= (size_t)ctx->head) {
        for (size_t i = 0; i < ctx->current_field_count; i++) {
            free(ctx->current_row[i]);
        }
        ctx->current_field_count = 0;
        ctx->current_row_num++;
        return;
    }

    if (ctx->tail > 0) {
        if (ctx->tail_buffer[ctx->tail_pos]) {
            for (size_t i = 0; i < ctx->tail_field_counts[ctx->tail_pos]; i++) {
                free(ctx->tail_buffer[ctx->tail_pos][i]);
            }
            free(ctx->tail_buffer[ctx->tail_pos]);
        }

        ctx->tail_buffer[ctx->tail_pos] = ctx->current_row;
        ctx->tail_field_counts[ctx->tail_pos] = ctx->current_field_count;
        ctx->tail_pos = (ctx->tail_pos + 1) % ctx->tail;

        ctx->current_row = calloc(ctx->current_field_capacity, sizeof(char *));
        if (!ctx->current_row) {
            fprintf(stderr, "Failed to allocate memory for new row\n");
            exit(1);
        }
        ctx->current_field_count = 0;
    } else {
        int first = 1;
        for (size_t i = 0; i < ctx->current_field_count; i++) {
            int should_output = 1;

            if (ctx->select_cols && ctx->select_count > 0) {
                should_output = 0;
                for (int j = 0; j < ctx->select_count; j++) {
                    if (ctx->select_cols[j] == (int)i) {
                        should_output = 1;
                        break;
                    }
                }
            }

            if (should_output) {
                if (!first) fprintf(ctx->output, "%c", ctx->config->delimiter);
                fprintf(ctx->output, "%s", ctx->current_row[i]);
                first = 0;
            }

            free(ctx->current_row[i]);
        }
        fprintf(ctx->output, "\n");
        ctx->current_field_count = 0;
    }

    ctx->row_count++;
    ctx->current_row_num++;
}

static void error_callback(void *user, int line, const char *msg) {
    cli_context *ctx = (cli_context *)user;
    if (!ctx->config->skip_lines_with_error) {
        fprintf(stderr, "Error at line %d: %s\n", line, msg);
    }
}

static void print_help(const char *prog) {
    printf("cisv - High-performance CSV parser\n\n");
    printf("Usage: %s [COMMAND] [OPTIONS] [FILE]\n\n", prog);
    printf("Commands:\n");
    printf("  parse    Parse CSV file (default if no command given)\n");
    printf("  write    Write/generate CSV files\n\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -d, --delimiter DELIM   Field delimiter (default: ,)\n");
    printf("  -q, --quote CHAR        Quote character (default: \")\n");
    printf("  -e, --escape CHAR       Escape character (default: RFC4180 style)\n");
    printf("  -m, --comment CHAR      Comment character (default: none)\n");
    printf("  -t, --trim              Trim whitespace from fields\n");
    printf("  -r, --relaxed           Use relaxed parsing rules\n");
    printf("  --skip-empty            Skip empty lines\n");
    printf("  --skip-errors           Skip lines with parse errors\n");
    printf("  --max-row SIZE          Maximum row size in bytes\n");
    printf("  --from-line N           Start from line N (1-based)\n");
    printf("  --to-line N             Stop at line N\n");
    printf("  -s, --select COLS       Select columns (comma-separated indices)\n");
    printf("  -c, --count             Show only row count\n");
    printf("  --head N                Show first N rows\n");
    printf("  --tail N                Show last N rows\n");
    printf("  -o, --output FILE       Write to FILE instead of stdout\n");
    printf("  -b, --benchmark         Run benchmark mode\n");
    printf("\nExamples:\n");
    printf("  %s data.csv                    # Parse and display CSV\n", prog);
    printf("  %s -c data.csv                 # Count rows\n", prog);
    printf("  %s -d ';' -q '\\'' data.csv     # Use semicolon delimiter\n", prog);
    printf("  %s -t --skip-empty data.csv    # Trim fields and skip empty lines\n", prog);
    printf("\nFor write options, use: %s write --help\n", prog);
}

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void benchmark_file(const char *filename, cisv_config *config) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    double size_mb = size / (1024.0 * 1024.0);
    printf("Benchmarking file: %s\n", filename);
    printf("File size: %.2f MB\n", size_mb);
    printf("Configuration: delimiter='%c', quote='%c', trim=%s, skip_empty=%s\n\n",
           config->delimiter, config->quote,
           config->trim ? "yes" : "no",
           config->skip_empty_lines ? "yes" : "no");

    const int iterations = 5;
    for (int i = 0; i < iterations; i++) {
        double start = get_time_ms();
        size_t count = cisv_parser_count_rows_with_config(filename, config);
        double end = get_time_ms();

        double throughput = size_mb / ((end - start) / 1000.0);
        printf("Run %d: %.2f ms, %zu rows, %.2f MB/s\n",
               i + 1, end - start, count, throughput);
    }
}

// Writer CLI implementation
#define DEFAULT_BUFFER_SIZE (1 << 20)

typedef enum {
    MODE_GENERATE,
    MODE_TRANSFORM,
    MODE_CONVERT
} write_mode_t;

static void print_write_help(const char *prog) {
    printf("cisv write - High-performance CSV writer\n\n");
    printf("Usage: %s write [OPTIONS]\n\n", prog);
    printf("Modes:\n");
    printf("  -g, --generate N        Generate N rows of test data\n");
    printf("  -t, --transform FILE    Transform existing CSV\n");
    printf("  -j, --json FILE         Convert JSON to CSV\n\n");
    printf("Options:\n");
    printf("  -o, --output FILE       Output file (default: stdout)\n");
    printf("  -d, --delimiter CHAR    Field delimiter (default: ,)\n");
    printf("  -q, --quote CHAR        Quote character (default: \")\n");
    printf("  -Q, --always-quote      Always quote fields\n");
    printf("  -r, --crlf              Use CRLF line endings\n");
    printf("  -n, --null STRING       String for NULL values (default: empty)\n");
    printf("  -c, --columns LIST      Column names for generation\n");
    printf("  -b, --benchmark         Run in benchmark mode\n");
}

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static int generate_csv(cisv_writer *writer, size_t rows, const char *columns) {
    (void)columns;
    const char *default_cols[] = {"id", "name", "email", "value", "timestamp"};
    size_t col_count = 5;

    if (cisv_writer_row(writer, default_cols, col_count) < 0) return -1;

    char buffer[256];
    for (size_t i = 0; i < rows; i++) {
        if (cisv_writer_field_int(writer, i + 1) < 0) return -1;

        snprintf(buffer, sizeof(buffer), "User_%zu", i);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        snprintf(buffer, sizeof(buffer), "user%zu@example.com", i);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        if (cisv_writer_field_double(writer, (double)(i * 1.23), 2) < 0) return -1;

        time_t now = time(NULL) + i;
        struct tm *tm = localtime(&now);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        if (cisv_writer_row_end(writer) < 0) return -1;

        if ((i + 1) % 1000000 == 0) {
            fprintf(stderr, "Generated %zu rows...\n", i + 1);
        }
    }

    return 0;
}

static int cisv_writer_main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"generate", required_argument, 0, 'g'},
        {"transform", required_argument, 0, 't'},
        {"json", required_argument, 0, 'j'},
        {"output", required_argument, 0, 'o'},
        {"delimiter", required_argument, 0, 'd'},
        {"quote", required_argument, 0, 'q'},
        {"always-quote", no_argument, 0, 'Q'},
        {"crlf", no_argument, 0, 'r'},
        {"null", required_argument, 0, 'n'},
        {"columns", required_argument, 0, 'c'},
        {"benchmark", no_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    write_mode_t mode = MODE_GENERATE;
    size_t generate_rows = 0;
    const char *output_file = NULL;
    const char *columns = NULL;
    int benchmark = 0;

    cisv_writer_config config = {
        .delimiter = ',',
        .quote_char = '"',
        .always_quote = 0,
        .use_crlf = 0,
        .null_string = "",
        .buffer_size = DEFAULT_BUFFER_SIZE
    };

    optind = 1;  // Reset getopt
    int opt;
    while ((opt = getopt_long(argc, argv, "g:t:j:o:d:q:Qrn:c:bh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'g': {
                mode = MODE_GENERATE;
                char *endptr;
                errno = 0;
                unsigned long long val = strtoull(optarg, &endptr, 10);
                if (errno == ERANGE || *endptr != '\0' || val == 0) {
                    fprintf(stderr, "Error: Invalid row count '%s'\n", optarg);
                    return 1;
                }
                // Reasonable limit to prevent accidental resource exhaustion
                if (val > 1000000000ULL) {
                    fprintf(stderr, "Error: Row count too large (max 1 billion)\n");
                    return 1;
                }
                generate_rows = (size_t)val;
                break;
            }
            case 't':
                mode = MODE_TRANSFORM;
                break;
            case 'j':
                mode = MODE_CONVERT;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'd':
                config.delimiter = optarg[0];
                break;
            case 'q':
                config.quote_char = optarg[0];
                break;
            case 'Q':
                config.always_quote = 1;
                break;
            case 'r':
                config.use_crlf = 1;
                break;
            case 'n':
                config.null_string = optarg;
                break;
            case 'c':
                columns = optarg;
                break;
            case 'b':
                benchmark = 1;
                break;
            case 'h':
                print_write_help(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Try '%s write --help' for more information.\n", argv[0]);
                return 1;
        }
    }

    FILE *output = stdout;
    if (output_file) {
        output = fopen(output_file, "wb");
        if (!output) {
            perror("fopen");
            return 1;
        }
    }

    cisv_writer *writer = cisv_writer_create_config(output, &config);
    if (!writer) {
        fprintf(stderr, "Failed to create writer\n");
        if (output != stdout) fclose(output);
        return 1;
    }

    double start_time = 0;
    if (benchmark) {
        start_time = get_time_seconds();
    }

    int result = 0;
    switch (mode) {
        case MODE_GENERATE:
            if (generate_rows == 0) {
                fprintf(stderr, "Error: Must specify number of rows to generate\n");
                result = 1;
            } else {
                result = generate_csv(writer, generate_rows, columns);
            }
            break;

        case MODE_TRANSFORM:
        case MODE_CONVERT:
            fprintf(stderr, "Transform/convert modes not yet implemented\n");
            result = 1;
            break;
    }

    cisv_writer_flush(writer);

    if (benchmark && result == 0) {
        double elapsed = get_time_seconds() - start_time;
        size_t bytes = cisv_writer_bytes_written(writer);
        size_t rows = cisv_writer_rows_written(writer);
        double mb = bytes / (1024.0 * 1024.0);
        double throughput = mb / elapsed;

        fprintf(stderr, "\nBenchmark Results:\n");
        fprintf(stderr, "  Rows written: %zu\n", rows);
        fprintf(stderr, "  Bytes written: %zu (%.2f MB)\n", bytes, mb);
        fprintf(stderr, "  Time: %.3f seconds\n", elapsed);
        fprintf(stderr, "  Throughput: %.2f MB/s\n", throughput);
        fprintf(stderr, "  Rows/sec: %.0f\n", rows / elapsed);
    }

    cisv_writer_destroy(writer);
    if (output != stdout) fclose(output);

    return result;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "write") == 0) {
        return cisv_writer_main(argc - 1, argv + 1);
    }

    if (argc > 1 && strcmp(argv[1], "parse") == 0) {
        argc--;
        argv++;
    }

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"delimiter", required_argument, 0, 'd'},
        {"quote", required_argument, 0, 'q'},
        {"escape", required_argument, 0, 'e'},
        {"comment", required_argument, 0, 'm'},
        {"trim", no_argument, 0, 't'},
        {"relaxed", no_argument, 0, 'r'},
        {"skip-empty", no_argument, 0, 1},
        {"skip-errors", no_argument, 0, 2},
        {"max-row", required_argument, 0, 3},
        {"from-line", required_argument, 0, 4},
        {"to-line", required_argument, 0, 5},
        {"select", required_argument, 0, 's'},
        {"count", no_argument, 0, 'c'},
        {"head", required_argument, 0, 6},
        {"tail", required_argument, 0, 7},
        {"output", required_argument, 0, 'o'},
        {"benchmark", no_argument, 0, 'b'},
        {0, 0, 0, 0}
    };

    cisv_config config;
    cisv_config_init(&config);

    cli_context ctx = {0};
    ctx.output = stdout;
    ctx.current_field_capacity = 16;
    ctx.current_row = calloc(ctx.current_field_capacity, sizeof(char *));
    ctx.config = &config;

    if (!ctx.current_row) {
        fprintf(stderr, "Failed to allocate initial row buffer\n");
        return 1;
    }

    int opt;
    int option_index = 0;
    const char *filename = NULL;
    const char *output_file = NULL;
    int benchmark = 0;

    while ((opt = getopt_long(argc, argv, "hvd:q:e:m:trs:co:b", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                free(ctx.current_row);
                return 0;

            case 'v':
                printf("cisv version 0.0.7\n");
                printf("Features: configurable parsing, SIMD optimizations\n");
                free(ctx.current_row);
                return 0;

            case 'd':
                // SECURITY: Validate delimiter
                if (optarg[0] == '\n' || optarg[0] == '\r') {
                    fprintf(stderr, "Error: Delimiter cannot be a newline character\n");
                    free(ctx.current_row);
                    return 1;
                }
                if (optarg[0] == '\0') {
                    fprintf(stderr, "Error: Delimiter cannot be empty\n");
                    free(ctx.current_row);
                    return 1;
                }
                config.delimiter = optarg[0];
                break;

            case 'q':
                // SECURITY: Validate quote character
                if (optarg[0] == '\n' || optarg[0] == '\r') {
                    fprintf(stderr, "Error: Quote character cannot be a newline character\n");
                    free(ctx.current_row);
                    return 1;
                }
                if (optarg[0] == '\0') {
                    fprintf(stderr, "Error: Quote character cannot be empty\n");
                    free(ctx.current_row);
                    return 1;
                }
                config.quote = optarg[0];
                break;

            case 'e':
                // SECURITY: Validate escape character
                if (optarg[0] == '\n' || optarg[0] == '\r') {
                    fprintf(stderr, "Error: Escape character cannot be a newline character\n");
                    free(ctx.current_row);
                    return 1;
                }
                config.escape = optarg[0];
                break;

            case 'm':
                config.comment = optarg[0];
                break;

            case 't':
                config.trim = true;
                break;

            case 'r':
                config.relaxed = true;
                break;

            case 1:
                config.skip_empty_lines = true;
                break;

            case 2:
                config.skip_lines_with_error = true;
                break;

            case 3: {
                long max_row;
                if (safe_parse_long(optarg, &max_row, 0) != 0) {
                    free(ctx.current_row);
                    return 1;
                }
                config.max_row_size = (size_t)max_row;
                break;
            }

            case 4: {
                int from_line;
                if (safe_parse_int(optarg, &from_line, 0) != 0) {
                    free(ctx.current_row);
                    return 1;
                }
                config.from_line = from_line;
                break;
            }

            case 5: {
                int to_line;
                if (safe_parse_int(optarg, &to_line, 0) != 0) {
                    free(ctx.current_row);
                    return 1;
                }
                config.to_line = to_line;
                break;
            }

            case 's': {
                char *cols_copy = strdup(optarg);
                if (!cols_copy) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(ctx.current_row);
                    return 1;
                }

                int count = 1;
                for (char *p = cols_copy; *p; p++) {
                    if (*p == ',') count++;
                }

                ctx.select_cols = calloc(count, sizeof(int));
                if (!ctx.select_cols) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(cols_copy);
                    free(ctx.current_row);
                    return 1;
                }

                char *tok = strtok(cols_copy, ",");
                int i = 0;
                while (tok && i < count) {
                    int col_idx;
                    if (safe_parse_int(tok, &col_idx, 0) != 0) {
                        fprintf(stderr, "Error: Invalid column index '%s'\n", tok);
                        free(cols_copy);
                        free(ctx.select_cols);
                        free(ctx.current_row);
                        return 1;
                    }
                    ctx.select_cols[i++] = col_idx;
                    tok = strtok(NULL, ",");
                }
                ctx.select_count = i;

                free(cols_copy);
                break;
            }

            case 'c':
                ctx.count_only = 1;
                break;

            case 'o':
                output_file = optarg;
                break;

            case 'b':
                benchmark = 1;
                break;

            case 6: {
                int head_val;
                if (safe_parse_int(optarg, &head_val, 0) != 0) {
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                // SECURITY: Limit head value to prevent excessive memory allocation
                if (head_val > 10000000) {
                    fprintf(stderr, "Error: --head value too large (max 10000000)\n");
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                ctx.head = head_val;
                break;
            }

            case 7: {
                int tail_val;
                if (safe_parse_int(optarg, &tail_val, 0) != 0) {
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                // SECURITY: Limit tail value to prevent excessive memory allocation
                // tail buffer allocates tail * sizeof(char**) bytes
                if (tail_val > 10000000) {
                    fprintf(stderr, "Error: --tail value too large (max 10000000)\n");
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                ctx.tail = tail_val;
                ctx.tail_buffer = calloc(ctx.tail, sizeof(char **));
                ctx.tail_field_counts = calloc(ctx.tail, sizeof(size_t));
                if (!ctx.tail_buffer || !ctx.tail_field_counts) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                break;
            }

            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                free(ctx.current_row);
                free(ctx.select_cols);
                return 1;
        }
    }

    // SECURITY: Validate configuration for conflicts after all options are parsed
    if (config.delimiter == config.quote) {
        fprintf(stderr, "Error: Delimiter and quote character cannot be the same ('%c')\n",
                config.delimiter);
        free(ctx.current_row);
        free(ctx.select_cols);
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
        return 1;
    }
    if (config.escape != '\0' && config.escape == config.delimiter) {
        fprintf(stderr, "Error: Escape and delimiter cannot be the same ('%c')\n",
                config.escape);
        free(ctx.current_row);
        free(ctx.select_cols);
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
        return 1;
    }

    if (optind < argc) {
        filename = argv[optind];
    }

    if (!filename) {
        fprintf(stderr, "Error: No input file specified\n");
        print_help(argv[0]);
        free(ctx.current_row);
        free(ctx.select_cols);
        return 1;
    }

    if (benchmark) {
        benchmark_file(filename, &config);
        free(ctx.current_row);
        free(ctx.select_cols);
        return 0;
    }

    if (ctx.count_only) {
        size_t count = cisv_parser_count_rows_with_config(filename, &config);
        printf("%zu\n", count);
        free(ctx.current_row);
        free(ctx.select_cols);
        return 0;
    }

    if (output_file) {
        ctx.output = fopen(output_file, "w");
        if (!ctx.output) {
            perror("fopen");
            free(ctx.current_row);
            free(ctx.select_cols);
            free(ctx.tail_buffer);
            free(ctx.tail_field_counts);
            return 1;
        }
    }

    config.field_cb = field_callback;
    config.row_cb = row_callback;
    config.error_cb = error_callback;
    config.user = &ctx;

    cisv_parser *parser = cisv_parser_create_with_config(&config);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        free(ctx.current_row);
        free(ctx.select_cols);
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
        if (ctx.output != stdout) fclose(ctx.output);
        return 1;
    }

    int result = cisv_parser_parse_file(parser, filename);
    if (result < 0) {
        fprintf(stderr, "Parse error: %s\n", strerror(-result));
        cisv_parser_destroy(parser);
        free(ctx.current_row);
        free(ctx.select_cols);
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
        if (ctx.output != stdout) fclose(ctx.output);
        return 1;
    }

    if (ctx.tail > 0 && ctx.tail_buffer) {
        size_t start = ctx.tail_pos;
        for (int i = 0; i < ctx.tail; i++) {
            size_t idx = (start + i) % ctx.tail;
            if (!ctx.tail_buffer[idx]) continue;

            int first = 1;
            for (size_t j = 0; j < ctx.tail_field_counts[idx]; j++) {
                if (!first) fprintf(ctx.output, "%c", config.delimiter);
                fprintf(ctx.output, "%s", ctx.tail_buffer[idx][j]);
                free(ctx.tail_buffer[idx][j]);
                first = 0;
            }
            fprintf(ctx.output, "\n");
            free(ctx.tail_buffer[idx]);
        }
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
    }

    if (getenv("CISV_STATS")) {
        fprintf(stderr, "Rows processed: %zu\n", ctx.row_count);
        fprintf(stderr, "Current line: %d\n", cisv_parser_get_line_number(parser));
    }

    cisv_parser_destroy(parser);
    free(ctx.current_row);
    free(ctx.select_cols);

    if (ctx.output != stdout) {
        fclose(ctx.output);
    }

    return 0;
}
