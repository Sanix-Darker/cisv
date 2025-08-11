#define _GNU_SOURCE
#include "cisv_writer.h"
#include "cisv_simd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#define DEFAULT_BUFFER_SIZE (1 << 20)  // 1MB
#define MIN_BUFFER_SIZE (1 << 16)      // 64KB

struct cisv_writer {
    FILE *output;
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_pos;

    // Configuration
    char delimiter;
    char quote_char;
    int always_quote;
    int use_crlf;
    const char *null_string;

    // State
    int in_field;
    size_t field_count;

    // Statistics
    size_t bytes_written;
    size_t rows_written;
};

// Check if field needs quoting
static inline int needs_quoting(const char *data, size_t len, char delim, char quote) {
    // SIMD-accelerated quote detection for x86-64
#if defined(cisv_ARCH_AVX512) || defined(cisv_ARCH_AVX2)
    const uint8_t *cur = (const uint8_t *)data;
    const uint8_t *end = cur + len;

#ifdef cisv_ARCH_AVX512
    const __m512i delim_vec = _mm512_set1_epi8(delim);
    const __m512i quote_vec = _mm512_set1_epi8(quote);
    const __m512i cr_vec = _mm512_set1_epi8('\r');
    const __m512i lf_vec = _mm512_set1_epi8('\n');

    while (cur + 64 <= end) {
        __m512i chunk = _mm512_loadu_si512((const __m512i*)cur);
        __mmask64 delim_mask = _mm512_cmpeq_epi8_mask(chunk, delim_vec);
        __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_vec);
        __mmask64 cr_mask = _mm512_cmpeq_epi8_mask(chunk, cr_vec);
        __mmask64 lf_mask = _mm512_cmpeq_epi8_mask(chunk, lf_vec);

        if (delim_mask | quote_mask | cr_mask | lf_mask) {
            return 1;
        }
        cur += 64;
    }
#elif defined(cisv_ARCH_AVX2)
    const __m256i delim_vec = _mm256_set1_epi8(delim);
    const __m256i quote_vec = _mm256_set1_epi8(quote);
    const __m256i cr_vec = _mm256_set1_epi8('\r');
    const __m256i lf_vec = _mm256_set1_epi8('\n');

    while (cur + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)cur);
        __m256i delim_cmp = _mm256_cmpeq_epi8(chunk, delim_vec);
        __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, quote_vec);
        __m256i cr_cmp = _mm256_cmpeq_epi8(chunk, cr_vec);
        __m256i lf_cmp = _mm256_cmpeq_epi8(chunk, lf_vec);

        __m256i any_match = _mm256_or_si256(
            _mm256_or_si256(delim_cmp, quote_cmp),
            _mm256_or_si256(cr_cmp, lf_cmp)
        );

        if (_mm256_movemask_epi8(any_match)) {
            return 1;
        }
        cur += 32;
    }
#endif

    // Handle remaining bytes
    while (cur < end) {
        char c = *cur++;
        if (c == delim || c == quote || c == '\r' || c == '\n') {
            return 1;
        }
    }
    return 0;
#else
    // Scalar fallback
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == delim || c == quote || c == '\r' || c == '\n') {
            return 1;
        }
    }
    return 0;
#endif
}

// Ensure buffer has at least 'needed' bytes available
static int ensure_buffer_space(cisv_writer *writer, size_t needed) {
    if (writer->buffer_pos + needed > writer->buffer_size) {
        if (cisv_writer_flush(writer) < 0) {
            return -1;
        }
        // If single field is larger than buffer, write directly
        if (needed > writer->buffer_size) {
            return -2;  // Signal direct write needed
        }
    }
    return 0;
}

// Write data to buffer
static void buffer_write(cisv_writer *writer, const void *data, size_t len) {
    memcpy(writer->buffer + writer->buffer_pos, data, len);
    writer->buffer_pos += len;
}

// Write escaped field to buffer
static int write_quoted_field(cisv_writer *writer, const char *data, size_t len) {
    // Worst case: all quotes need escaping + 2 quotes
    size_t max_size = len * 2 + 2;

    int space_result = ensure_buffer_space(writer, max_size);
    if (space_result == -2) {
        // Field too large for buffer, write directly
        if (fputc(writer->quote_char, writer->output) == EOF) return -1;

        for (size_t i = 0; i < len; i++) {
            if (data[i] == writer->quote_char) {
                if (fputc(writer->quote_char, writer->output) == EOF) return -1;
            }
            if (fputc(data[i], writer->output) == EOF) return -1;
        }

        if (fputc(writer->quote_char, writer->output) == EOF) return -1;
        writer->bytes_written += len + 2;
        return 0;
    } else if (space_result < 0) {
        return -1;
    }

    // Write to buffer
    writer->buffer[writer->buffer_pos++] = writer->quote_char;

    for (size_t i = 0; i < len; i++) {
        if (data[i] == writer->quote_char) {
            writer->buffer[writer->buffer_pos++] = writer->quote_char;
        }
        writer->buffer[writer->buffer_pos++] = data[i];
    }

    writer->buffer[writer->buffer_pos++] = writer->quote_char;
    return 0;
}

cisv_writer *cisv_writer_create(FILE *output) {
    cisv_writer_config config = {
        .delimiter = ',',
        .quote_char = '"',
        .always_quote = 0,
        .use_crlf = 0,
        .null_string = "",
        .buffer_size = DEFAULT_BUFFER_SIZE
    };
    return cisv_writer_create_config(output, &config);
}

cisv_writer *cisv_writer_create_config(FILE *output, const cisv_writer_config *config) {
    if (!output) return NULL;

    cisv_writer *writer = calloc(1, sizeof(*writer));
    if (!writer) return NULL;

    writer->buffer_size = config->buffer_size;
    if (writer->buffer_size < MIN_BUFFER_SIZE) {
        writer->buffer_size = MIN_BUFFER_SIZE;
    }

    writer->buffer = malloc(writer->buffer_size);
    if (!writer->buffer) {
        free(writer);
        return NULL;
    }

    writer->output = output;
    writer->delimiter = config->delimiter;
    writer->quote_char = config->quote_char;
    writer->always_quote = config->always_quote;
    writer->use_crlf = config->use_crlf;
    writer->null_string = config->null_string ? config->null_string : "";

    return writer;
}

void cisv_writer_destroy(cisv_writer *writer) {
    if (!writer) return;

    cisv_writer_flush(writer);
    free(writer->buffer);
    free(writer);
}

int cisv_writer_field(cisv_writer *writer, const char *data, size_t len) {
    if (!writer) return -1;

    // Add delimiter if not first field
    if (writer->field_count > 0) {
        if (ensure_buffer_space(writer, 1) < 0) return -1;
        writer->buffer[writer->buffer_pos++] = writer->delimiter;
    }

    // Handle NULL
    if (!data) {
        data = writer->null_string;
        len = strlen(data);
    }

    // Check if quoting needed
    if (writer->always_quote || needs_quoting(data, len, writer->delimiter, writer->quote_char)) {
        if (write_quoted_field(writer, data, len) < 0) return -1;
    } else {
        // Simple field - write directly
        int space_result = ensure_buffer_space(writer, len);
        if (space_result == -2) {
            // Direct write
            if (fwrite(data, 1, len, writer->output) != len) return -1;
            writer->bytes_written += len;
        } else if (space_result < 0) {
            return -1;
        } else {
            buffer_write(writer, data, len);
        }
    }

    writer->field_count++;
    writer->in_field = 0;
    return 0;
}

int cisv_writer_field_str(cisv_writer *writer, const char *str) {
    if (!str) return cisv_writer_field(writer, NULL, 0);
    return cisv_writer_field(writer, str, strlen(str));
}

int cisv_writer_field_int(cisv_writer *writer, int64_t value) {
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return cisv_writer_field(writer, buffer, len);
}

int cisv_writer_field_double(cisv_writer *writer, double value, int precision) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return cisv_writer_field(writer, buffer, len);
}

int cisv_writer_row_end(cisv_writer *writer) {
    if (!writer) return -1;

    // Write line ending
    if (writer->use_crlf) {
        if (ensure_buffer_space(writer, 2) < 0) return -1;
        writer->buffer[writer->buffer_pos++] = '\r';
        writer->buffer[writer->buffer_pos++] = '\n';
    } else {
        if (ensure_buffer_space(writer, 1) < 0) return -1;
        writer->buffer[writer->buffer_pos++] = '\n';
    }

    writer->field_count = 0;
    writer->rows_written++;
    return 0;
}

int cisv_writer_row(cisv_writer *writer, const char **fields, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (cisv_writer_field_str(writer, fields[i]) < 0) return -1;
    }
    return cisv_writer_row_end(writer);
}

int cisv_writer_flush(cisv_writer *writer) {
    if (!writer || writer->buffer_pos == 0) return 0;

    if (fwrite(writer->buffer, 1, writer->buffer_pos, writer->output) != writer->buffer_pos) {
        return -1;
    }

    writer->bytes_written += writer->buffer_pos;
    writer->buffer_pos = 0;
    return 0;
}

size_t cisv_writer_bytes_written(const cisv_writer *writer) {
    return writer ? writer->bytes_written + writer->buffer_pos : 0;
}

size_t cisv_writer_rows_written(const cisv_writer *writer) {
    return writer ? writer->rows_written : 0;
}

// CLI integration
#ifdef CISV_CLI

#include <getopt.h>
#include <sys/time.h>
#include <time.h>

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

static double get_time_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Generate test data
static int generate_csv(cisv_writer *writer, size_t rows, const char *columns) {
    (void)columns; // TODO: implement custom columns support
    // Default columns if not specified
    const char *default_cols[] = {"id", "name", "email", "value", "timestamp"};
    size_t col_count = 5;

    // Write header
    if (cisv_writer_row(writer, default_cols, col_count) < 0) return -1;

    // Generate rows
    char buffer[256];
    for (size_t i = 0; i < rows; i++) {
        // ID
        if (cisv_writer_field_int(writer, i + 1) < 0) return -1;

        // Name
        snprintf(buffer, sizeof(buffer), "User_%zu", i);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        // Email
        snprintf(buffer, sizeof(buffer), "user%zu@example.com", i);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        // Value
        if (cisv_writer_field_double(writer, (double)(i * 1.23), 2) < 0) return -1;

        // Timestamp
        time_t now = time(NULL) + i;
        struct tm *tm = localtime(&now);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
        if (cisv_writer_field_str(writer, buffer) < 0) return -1;

        if (cisv_writer_row_end(writer) < 0) return -1;

        // Progress report every 1M rows
        if ((i + 1) % 1000000 == 0) {
            fprintf(stderr, "Generated %zu rows...\n", i + 1);
        }
    }

    return 0;
}

int cisv_writer_main(int argc, char *argv[]) {
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
    //const char *input_file = NULL;
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

    int opt;
    while ((opt = getopt_long(argc, argv, "g:t:j:o:d:q:Qrn:c:bh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'g':
                mode = MODE_GENERATE;
                generate_rows = strtoull(optarg, NULL, 10);
                break;
            case 't':
                mode = MODE_TRANSFORM;
                //input_file = optarg;
                break;
            case 'j':
                mode = MODE_CONVERT;
                //input_file = optarg;
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

    // Open output file
    FILE *output = stdout;
    if (output_file) {
        output = fopen(output_file, "wb");
        if (!output) {
            perror("fopen");
            return 1;
        }
    }

    // Create writer
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

#endif // CISV_CLI
