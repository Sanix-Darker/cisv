#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win_getopt.h"
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#ifdef _WIN32
    #include <windows.h>

    #ifndef WIN_GETOPT_H
    #define WIN_GETOPT_H

    #include <string.h>
    #include <stdio.h>

    // Global variables
    char* optarg = NULL;    // Current option argument
    int optind = 1;         // Index of next argument to process
    int opterr = 1;         // Enable error messages (default: 1)
    int optopt = 0;         // Current option character

    // Reset getopt internal state
    void win_getopt_reset() {
        optarg = NULL;
        optind = 1;
        opterr = 1;
        optopt = 0;
    }

    int getopt(int argc, char* const argv[], const char* optstring) {
        static int optpos = 1;  // Position within current argument
        const char* colon = strchr(optstring, ':');

        // Reset for new scan
        optarg = NULL;

        // Argument boundary checks
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }

        // Handle "--" end of options
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }

        // Get current option character
        optopt = argv[optind][optpos++];

        // Find option in optstring
        char* optptr = strchr(optstring, optopt);

        // Unknown option
        if (!optptr) {
            if (opterr && *optstring != ':')
                fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], optopt);
            return '?';
        }

        // Handle required argument
        if (optptr[1] == ':') {
            // Argument in current token
            if (argv[optind][optpos] != '\0') {
                optarg = &argv[optind][optpos];
                optpos = 1;
                optind++;
            }
            // Argument in next token
            else if (argc > optind + 1) {
                optarg = argv[optind + 1];
                optind += 2;
                optpos = 1;
            }
            // Missing argument
            else {
                if (opterr && *optstring != ':') {
                    fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                            argv[0], optopt);
                }
                optpos = 1;
                optind++;
                return (colon && colon[1] == ':') ? ':' : '?';
            }
        }
        // No argument required
        else {
            if (argv[optind][optpos] == '\0') {
                optpos = 1;
                optind++;
            }
        }

        return optopt;
    }

    #endif // WIN_GETOPT_H
#elif defined(__linux__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <getopt.h>
    #include <sys/time.h>
#else
    #error "Unsupported platform"
#endif
#include "cisv_parser.h"

// NOTE: this is at compile time only
#define RINGBUF_SIZE (1 << 20) // 1 MiB (we may adjust according to needs)

struct cisv_parser {
    uint8_t *base;              // pointer to the whole input, if memory-mapped
    size_t size;                // length of that mapping
    int fd;                     // the underlying file descriptor (-1 ⇒ none)
    uint8_t *ring;              // malloc’ed circular buffer when not mmapped
    size_t head;                // write head: next byte slot to fill
    enum {
        S_UNQUOTED,             // Parsing ordinary characters outside quotes.

        S_QUOTED,               // Inside a quoted field. Delimiters/newlines are treated as data.

        S_QUOTE_ESC             // Just saw a ". Decide whether it’s a doubled quote ("" → stay in field) or
                                // the closing quote.
    } st;
    cisv_field_cb fcb;           // field callback fired whenever a full cell is ready
                                // (delimiter or row-ending newline encountered, consistent with RFC 4180 rules)

    cisv_row_cb rcb;             // row callback fired after the last field of each record
    void *user;
    const uint8_t *field_start; // where the in-progress field began
};

static inline void yield_field(cisv_parser *parser, const uint8_t *start, const uint8_t *end) {
    if (parser->fcb && start && end && end >= start) {
        parser->fcb(parser->user, (const char *)start, (size_t)(end - start));
    }
}

static inline void yield_row(cisv_parser *parser) {
    if (parser->rcb) {
        parser->rcb(parser->user);
    }
}

static void parse_scalar(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    for (; cur < end; ++cur) {
        uint8_t c = *cur;
        switch (parser->st) {
            case S_UNQUOTED:
                if (c == ',') {
                    yield_field(parser, parser->field_start, cur);
                    parser->field_start = cur + 1;
                } else if (c == '"') {
                    parser->st = S_QUOTED;
                } else if (c == '\n') {
                    yield_field(parser, parser->field_start, cur);
                    yield_row(parser);
                    parser->field_start = cur + 1;
                }
                break;
            case S_QUOTED:
                if (c == '"') parser->st = S_QUOTE_ESC;
                break;
            case S_QUOTE_ESC:
                if (c == '"') {
                    parser->st = S_QUOTED;
                } else {
                    parser->st = S_UNQUOTED;
                    cur--;
                }
                break;
        }
    }
}

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->st = S_UNQUOTED;
    parse_scalar(parser, buffer, len);

    if (parser->field_start < buffer + len) {
        yield_field(parser, parser->field_start, buffer + len);
    }
    return 0;
}

size_t cisv_parser_count_rows(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return 0;
    }

    if (st.st_size == 0) {
        close(fd);
        return 0;
    }

    uint8_t *base = (uint8_t *)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < (size_t)st.st_size; i++) {
        if (base[i] == '\n') count++;
    }

    munmap(base, st.st_size);
    close(fd);
    return count;
}

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user) {
    cisv_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    parser->ring = malloc(RINGBUF_SIZE + 64);
    if (!parser->ring) {
        free(parser);
        return NULL;
    }

    parser->fd = -1;
    parser->fcb = fcb;
    parser->rcb = rcb;
    parser->user = user;
    return parser;
}

void cisv_parser_destroy(cisv_parser *parser) {
    if (!parser) return;

    if (parser->base && parser->base != MAP_FAILED) {
        munmap(parser->base, parser->size);
    }
    if (parser->fd >= 0) {
        close(parser->fd);
    }
    free(parser->ring);
    free(parser);
}

int cisv_parser_parse_file(cisv_parser *parser, const char *path) {
    struct stat st;
    parser->fd = open(path, O_RDONLY);
    if (parser->fd < 0) return -errno;

    if (fstat(parser->fd, &st) < 0) {
        int err = errno;
        close(parser->fd);
        parser->fd = -1;
        return -err;
    }

    if (st.st_size == 0) {
        close(parser->fd);
        parser->fd = -1;
        return 0;
    }

    parser->size = st.st_size;
    parser->base = mmap(NULL, parser->size, PROT_READ, MAP_PRIVATE, parser->fd, 0);
    if (parser->base == MAP_FAILED) {
        int err = errno;
        close(parser->fd);
        parser->fd = -1;
        return -err;
    }

    return parse_memory(parser, parser->base, parser->size);
}

int cisv_parser_write(cisv_parser *parser, const uint8_t *chunk, size_t len) {
    if (!parser || !chunk || len >= RINGBUF_SIZE) return -EINVAL;

    if (parser->head + len > RINGBUF_SIZE) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }

    memcpy(parser->ring + parser->head, chunk, len);
    parser->head += len;

    if (memchr(chunk, '\n', len) || parser->head > (RINGBUF_SIZE / 2)) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }
    return 0;
}

void cisv_parser_end(cisv_parser *parser) {
    if (!parser || parser->head == 0) return;
    parse_memory(parser, parser->ring, parser->head);
    parser->head = 0;
}

#ifdef CISV_CLI

#include "cisv_writer.h"

// Forward declaration for writer
int cisv_writer_main(int argc, char *argv[]);

typedef struct {
    size_t row_count;
    size_t field_count;
    int count_only;
    int head;
    int tail;
    char delimiter;
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
} cli_context;

static void field_callback(void *user, const char *data, size_t len) {
    cli_context *ctx = (cli_context *)user;

    // Ensure we have space for the field
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

    // Allocate and copy field data
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

    // Skip row if we're past head limit
    if (ctx->head > 0 && ctx->current_row_num >= (size_t)ctx->head) {
        for (size_t i = 0; i < ctx->current_field_count; i++) {
            free(ctx->current_row[i]);
        }
        ctx->current_field_count = 0;
        ctx->current_row_num++;
        return;
    }

    // Handle tail buffering
    if (ctx->tail > 0) {
        // Free old row if buffer is full
        if (ctx->tail_buffer[ctx->tail_pos]) {
            for (size_t i = 0; i < ctx->tail_field_counts[ctx->tail_pos]; i++) {
                free(ctx->tail_buffer[ctx->tail_pos][i]);
            }
            free(ctx->tail_buffer[ctx->tail_pos]);
        }

        // Store current row in circular buffer
        ctx->tail_buffer[ctx->tail_pos] = ctx->current_row;
        ctx->tail_field_counts[ctx->tail_pos] = ctx->current_field_count;
        ctx->tail_pos = (ctx->tail_pos + 1) % ctx->tail;

        // Allocate new row
        ctx->current_row = calloc(ctx->current_field_capacity, sizeof(char *));
        if (!ctx->current_row) {
            fprintf(stderr, "Failed to allocate memory for new row\n");
            exit(1);
        }
        ctx->current_field_count = 0;
    } else {
        // Output row immediately
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
                if (!first) fprintf(ctx->output, "%c", ctx->delimiter);
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

static void print_help(const char *prog) {
    printf("cisv - The fastest CSV parser of the multiverse\n\n");
    printf("Usage: %s [COMMAND] [OPTIONS] [FILE]\n\n", prog);
    printf("Commands:\n");
    printf("  parse    Parse CSV file (default if no command given)\n");
    printf("  write    Write/generate CSV files\n\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -d, --delimiter DELIM   Field delimiter (default: ,)\n");
    printf("  -s, --select COLS       Select columns (comma-separated indices)\n");
    printf("  -c, --count             Show only row count\n");
    printf("  --head N                Show first N rows\n");
    printf("  --tail N                Show last N rows\n");
    printf("  -o, --output FILE       Write to FILE instead of stdout\n");
    printf("  -b, --benchmark         Run benchmark mode\n");
    printf("\n----------\nExamples:\n");
    printf("  %s data.csv                    # Parse and display CSV\n", prog);
    printf("  %s -c data.csv                 # Count rows\n", prog);
    printf("  %s -s 0,2,3 data.csv           # Select columns 0, 2, and 3\n", prog);
    printf("  %s --head 10 data.csv          # Show first 10 rows\n", prog);
    printf("  %s -d ';' data.csv             # Use semicolon as delimiter\n", prog);
    printf("\nFor write options, use: %s write --help\n", prog);
}

static double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void benchmark_file(const char *filename) {
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
    printf("File size: %.2f MB\n\n", size_mb);

    const int iterations = 5;
    for (int i = 0; i < iterations; i++) {
        double start = get_time_ms();
        size_t count = cisv_parser_count_rows(filename);
        double end = get_time_ms();

        printf("Run %d: %.2f ms, %zu rows\n", i + 1, end - start, count);
    }
}

int main(int argc, char *argv[]) {
    // Check for write command
    if (argc > 1 && strcmp(argv[1], "write") == 0) {
        // Shift arguments and call writer main
        return cisv_writer_main(argc - 1, argv + 1);
    }

    // If first arg is "parse", skip it
    if (argc > 1 && strcmp(argv[1], "parse") == 0) {
        argc--;
        argv++;
    }

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"delimiter", required_argument, 0, 'd'},
        {"select", required_argument, 0, 's'},
        {"count", no_argument, 0, 'c'},
        {"head", required_argument, 0, 1},
        {"tail", required_argument, 0, 2},
        {"output", required_argument, 0, 'o'},
        {"benchmark", no_argument, 0, 'b'},
        {0, 0, 0, 0}
    };

    // Initialize context
    cli_context ctx = {0};
    ctx.delimiter = ',';
    ctx.output = stdout;
    ctx.current_field_capacity = 16;
    ctx.current_row = calloc(ctx.current_field_capacity, sizeof(char *));
    if (!ctx.current_row) {
        fprintf(stderr, "Failed to allocate initial row buffer\n");
        return 1;
    }

    int opt;
    int option_index = 0;
    const char *filename = NULL;
    const char *output_file = NULL;
    int benchmark = 0;

    while ((opt = getopt_long(argc, argv, "hvd:s:co:b", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                free(ctx.current_row);
                return 0;

            case 'v':
                printf("cisv version 0.0.1\n");
                free(ctx.current_row);
                return 0;

            case 'd':
                ctx.delimiter = optarg[0];
                break;

            case 's': {
                // Parse column selection
                char *cols_copy = strdup(optarg);
                if (!cols_copy) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(ctx.current_row);
                    return 1;
                }

                // Count columns
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

                // Parse column numbers
                char *tok = strtok(cols_copy, ",");
                int i = 0;
                while (tok && i < count) {
                    ctx.select_cols[i++] = atoi(tok);
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

            case 1: // --head
                ctx.head = atoi(optarg);
                break;

            case 2: // --tail
                ctx.tail = atoi(optarg);
                ctx.tail_buffer = calloc(ctx.tail, sizeof(char **));
                ctx.tail_field_counts = calloc(ctx.tail, sizeof(size_t));
                if (!ctx.tail_buffer || !ctx.tail_field_counts) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(ctx.current_row);
                    free(ctx.select_cols);
                    return 1;
                }
                break;

            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                free(ctx.current_row);
                free(ctx.select_cols);
                return 1;
        }
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
        benchmark_file(filename);
        free(ctx.current_row);
        free(ctx.select_cols);
        return 0;
    }

    if (ctx.count_only) {
        size_t count = cisv_parser_count_rows(filename);
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

    cisv_parser *parser = cisv_parser_create(field_callback, row_callback, &ctx);
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

    // Output tail buffer if used
    if (ctx.tail > 0 && ctx.tail_buffer) {
        size_t start = ctx.tail_pos;
        for (int i = 0; i < ctx.tail; i++) {
            size_t idx = (start + i) % ctx.tail;
            if (!ctx.tail_buffer[idx]) continue;

            int first = 1;
            for (size_t j = 0; j < ctx.tail_field_counts[idx]; j++) {
                if (!first) fprintf(ctx.output, "%c", ctx.delimiter);
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

    cisv_parser_destroy(parser);
    free(ctx.current_row);
    free(ctx.select_cols);

    if (ctx.output != stdout) {
        fclose(ctx.output);
    }

    return 0;
}

#endif // CISV_CLI
