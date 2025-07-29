#define _GNU_SOURCE
#include "cisv_parser.h"
#include "cisv_simd.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <immintrin.h>

#define RINGBUF_SIZE (1 << 20)
#define PREFETCH_DISTANCE 256

struct cisv_parser {
    uint8_t *base;
    size_t size;
    int fd;
    uint8_t *ring;
    size_t head;
    enum {
        S_UNQUOTED,
        S_QUOTED,
        S_QUOTE_ESC
    } st;
    cisv_field_cb fcb;
    cisv_row_cb rcb;
    void *user;
    const uint8_t *field_start;
};

static inline void yield_field(
    cisv_parser *parser,
    const uint8_t *start,
    const uint8_t *end
) {
    if (!parser->fcb) return;
    parser->fcb(parser->user, (const char *)start, (size_t)(end - start));
}

static inline void yield_row(cisv_parser *parser) {
    if (!parser->rcb) return;
    parser->rcb(parser->user);
}

// Universal scalar parser
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

// Optimized SIMD parser for x86-64 with AVX2/AVX-512
#if defined(cisv_ARCH_AVX512) || defined(cisv_ARCH_AVX2)
static void parse_simd(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

#ifdef cisv_ARCH_AVX512
    const __m512i comma_vec = _mm512_set1_epi8(',');
    const __m512i quote_vec = _mm512_set1_epi8('"');
    const __m512i newline_vec = _mm512_set1_epi8('\n');
#else
    const __m256i comma_vec = _mm256_set1_epi8(',');
    const __m256i quote_vec = _mm256_set1_epi8('"');
    const __m256i newline_vec = _mm256_set1_epi8('\n');
#endif

    while (cur + cisv_VEC_BYTES <= end) {
        _mm_prefetch(cur + PREFETCH_DISTANCE, _MM_HINT_T0);

#ifdef cisv_ARCH_AVX512
        __m512i chunk = _mm512_loadu_si512((const __m512i*)cur);
        __mmask64 comma_mask = _mm512_cmpeq_epi8_mask(chunk, comma_vec);
        __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_vec);
        __mmask64 newline_mask = _mm512_cmpeq_epi8_mask(chunk, newline_vec);
#else
        __m256i chunk = _mm256_loadu_si256((const __m256i*)cur);
        uint32_t comma_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, comma_vec));
        uint32_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_vec));
        uint32_t newline_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, newline_vec));
#endif

        if (!quote_mask && parser->st == S_UNQUOTED) {
            while (comma_mask) {
                int pos = __builtin_ctzll(comma_mask);
                yield_field(parser, parser->field_start, cur + pos);
                parser->field_start = cur + pos + 1;
                comma_mask &= comma_mask - 1;
            }

            while (newline_mask) {
                int pos = __builtin_ctzll(newline_mask);
                yield_field(parser, parser->field_start, cur + pos);
                yield_row(parser);
                parser->field_start = cur + pos + 1;
                newline_mask &= newline_mask - 1;
            }

            cur += cisv_VEC_BYTES;
        } else {
            parse_scalar(parser, cur, cisv_VEC_BYTES);
            cur += cisv_VEC_BYTES;
        }
    }

    if (cur < end) {
        parse_scalar(parser, cur, end - cur);
    }
}
#endif

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->st = S_UNQUOTED;

#if defined(cisv_ARCH_AVX512) || defined(cisv_ARCH_AVX2)
    parse_simd(parser, buffer, len);
#else
    parse_scalar(parser, buffer, len);
#endif

    if (parser->field_start < buffer + len) {
        yield_field(parser, parser->field_start, buffer + len);
    }
    return 0;
}

// Optimized row counting - no field processing
size_t cisv_parser_count_rows(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st)) {
        close(fd);
        return 0;
    }

    uint8_t *base = (uint8_t *)mmap(NULL, st.st_size, PROT_READ,
                                     MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        return 0;
    }

#ifdef MADV_SEQUENTIAL
    madvise(base, st.st_size, MADV_SEQUENTIAL);
#endif

    size_t count = 0;
    const uint8_t *cur = base;
    const uint8_t *end = base + st.st_size;

#if defined(cisv_ARCH_AVX512)
    const __m512i newline_vec = _mm512_set1_epi8('\n');
    while (cur + 64 <= end) {
        _mm_prefetch(cur + 512, _MM_HINT_T0);
        __m512i chunk = _mm512_loadu_si512((const __m512i*)cur);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, newline_vec);
        count += __builtin_popcountll(mask);
        cur += 64;
    }
#elif defined(cisv_ARCH_AVX2)
    const __m256i newline_vec = _mm256_set1_epi8('\n');
    while (cur + 32 <= end) {
        _mm_prefetch(cur + 256, _MM_HINT_T0);
        __m256i chunk = _mm256_loadu_si256((const __m256i*)cur);
        uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, newline_vec));
        count += __builtin_popcount(mask);
        cur += 32;
    }
#endif

    // Handle remaining bytes
    while (cur < end) {
        if (*cur++ == '\n') count++;
    }

    munmap(base, st.st_size);
    close(fd);
    return count;
}

cisv_parser *cisv_parser_create(
    cisv_field_cb fcb,
    cisv_row_cb rcb,
    void *user
) {
    cisv_parser *parser = (cisv_parser *)calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    parser->ring = (uint8_t*)malloc(RINGBUF_SIZE + 64);
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

int cisv_parser_parse_file(
    cisv_parser *parser,
    const char *path
) {
    struct stat st;
    parser->fd = open(path, O_RDONLY);
    if (parser->fd < 0) return -errno;
    if (fstat(parser->fd, &st)) {
        close(parser->fd);
        return -errno;
    }
    parser->size = (size_t)st.st_size;

    parser->base = (uint8_t *)mmap(NULL, parser->size, PROT_READ,
                                   MAP_PRIVATE | MAP_POPULATE, parser->fd, 0);
    if (parser->base == MAP_FAILED) {
        close(parser->fd);
        return -errno;
    }

#ifdef MADV_SEQUENTIAL
    madvise(parser->base, parser->size, MADV_SEQUENTIAL);
#endif
#ifdef MADV_WILLNEED
    madvise(parser->base, parser->size, MADV_WILLNEED);
#endif

    return parse_memory(parser, parser->base, parser->size);
}

int cisv_parser_write(
    cisv_parser *parser,
    const uint8_t *chunk,
    size_t len
) {
    if (len >= RINGBUF_SIZE) return -EINVAL;

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
    if (parser->head <= 0) return;
    parse_memory(parser, parser->ring, parser->head);
    parser->head = 0;
}

// CLI code integrated into parser
#ifdef CISV_CLI

typedef struct {
    size_t row_count;
    size_t field_count;
    int count_only;
    int no_header;
    int benchmark;
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
} cli_context;

static void field_callback(void *user, const char *data, size_t len) {
    cli_context *ctx = (cli_context *)user;

    if (ctx->current_field_count >= ctx->current_field_capacity) {
        ctx->current_field_capacity = ctx->current_field_capacity ? ctx->current_field_capacity * 2 : 16;
        ctx->current_row = realloc(ctx->current_row, ctx->current_field_capacity * sizeof(char *));
    }

    ctx->current_row[ctx->current_field_count] = malloc(len + 1);
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

        ctx->current_row = malloc(ctx->current_field_capacity * sizeof(char *));
        ctx->current_field_count = 0;
    } else {
        for (size_t i = 0; i < ctx->current_field_count; i++) {
            if (ctx->select_cols) {
                int selected = 0;
                for (int j = 0; j < ctx->select_count; j++) {
                    if (ctx->select_cols[j] == (int)i) {
                        selected = 1;
                        break;
                    }
                }
                if (!selected) continue;
            }

            if (i > 0) fprintf(ctx->output, "%c", ctx->delimiter);
            fprintf(ctx->output, "%s", ctx->current_row[i]);
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
    printf("Usage: %s [OPTIONS] [FILE]\n\n", prog);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -d, --delimiter DELIM   Field delimiter (default: ,)\n");
    printf("  -n, --no-header         Treat first row as data, not header\n");
    printf("  -s, --select COLS       Select columns (comma-separated indices)\n");
    printf("  -c, --count             Show only row count\n");
    printf("  --head N                Show first N rows\n");
    printf("  --tail N                Show last N rows\n");
    printf("  -o, --output FILE       Write to FILE instead of stdout\n");
    printf("  -b, --benchmark         Run benchmark mode\n\n");
    printf("\nExamples:\n");
    printf("  %s data.csv                    # Parse and display CSV\n", prog);
    printf("  %s -c data.csv                 # Count rows\n", prog);
    printf("  %s -s 0,2,3 data.csv           # Select columns 0, 2, and 3\n", prog);
    printf("  %s --head 10 data.csv          # Show first 10 rows\n", prog);
    printf("  %s -d ';' data.csv             # Use semicolon as delimiter\n", prog);
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
    double times[iterations];
    size_t row_counts[iterations];

    for (int i = 0; i < iterations; i++) {
        double start = get_time_ms();
        size_t count = cisv_parser_count_rows(filename);
        double end = get_time_ms();

        times[i] = end - start;
        row_counts[i] = count;

        printf("Run %d: %.2f ms, %zu rows\n", i + 1, times[i], row_counts[i]);
    }

    double avg_time = 0;
    for (int i = 0; i < iterations; i++) {
        avg_time += times[i];
    }
    avg_time /= iterations;

    double throughput = size_mb / (avg_time / 1000.0);

    printf("\nAverage time: %.2f ms\n", avg_time);
    printf("Throughput: %.2f MB/s\n", throughput);
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"delimiter", required_argument, 0, 'd'},
        {"no-header", no_argument, 0, 'n'},
        {"select", required_argument, 0, 's'},
        {"count", no_argument, 0, 'c'},
        {"head", required_argument, 0, 0},
        {"tail", required_argument, 0, 1},
        {"output", required_argument, 0, 'o'},
        {"benchmark", no_argument, 0, 'b'},
        {0, 0, 0, 0}
    };

    cli_context ctx = {0};
    ctx.delimiter = ',';
    ctx.output = stdout;
    ctx.current_field_capacity = 16;
    ctx.current_row = malloc(ctx.current_field_capacity * sizeof(char *));

    int opt;
    int option_index = 0;
    const char *filename = NULL;
    const char *output_file = NULL;
    int benchmark = 0;

    while ((opt = getopt_long(argc, argv, "hvd:ns:co:b", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'v':
                printf("cisv version 0.0.1\n");
                return 0;
            case 'd':
                ctx.delimiter = optarg[0];
                break;
            case 'n':
                ctx.no_header = 1;
                break;
            case 's': {
                char *tok = strtok(optarg, ",");
                int count = 0;
                while (tok) {
                    count++;
                    tok = strtok(NULL, ",");
                }
                ctx.select_cols = malloc(count * sizeof(int));
                ctx.select_count = count;
                tok = strtok(optarg, ",");
                for (int i = 0; i < count; i++) {
                    ctx.select_cols[i] = atoi(tok);
                    tok = strtok(NULL, ",");
                }
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
            case 0: // --head
                ctx.head = atoi(optarg);
                break;
            case 1: // --tail
                ctx.tail = atoi(optarg);
                ctx.tail_buffer = calloc(ctx.tail, sizeof(char **));
                ctx.tail_field_counts = calloc(ctx.tail, sizeof(size_t));
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        filename = argv[optind];
    }

    if (!filename) {
        fprintf(stderr, "Error: No input file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (benchmark) {
        benchmark_file(filename);
        return 0;
    }

    // Fast path for counting
    if (ctx.count_only) {
        size_t count = cisv_parser_count_rows(filename);
        printf("%zu\n", count);
        return 0;
    }

    if (output_file) {
        ctx.output = fopen(output_file, "w");
        if (!ctx.output) {
            perror("fopen");
            return 1;
        }
    }

    cisv_parser *parser = cisv_parser_create(field_callback, row_callback, &ctx);
    int result = cisv_parser_parse_file(parser, filename);

    if (result < 0) {
        fprintf(stderr, "Error: %s\n", strerror(-result));
        cisv_parser_destroy(parser);
        return 1;
    }

    if (ctx.tail > 0) {
        size_t start = ctx.tail_pos;
        for (int i = 0; i < ctx.tail; i++) {
            size_t idx = (start + i) % ctx.tail;
            if (!ctx.tail_buffer[idx]) continue;

            for (size_t j = 0; j < ctx.tail_field_counts[idx]; j++) {
                if (j > 0) fprintf(ctx.output, "%c", ctx.delimiter);
                fprintf(ctx.output, "%s", ctx.tail_buffer[idx][j]);
                free(ctx.tail_buffer[idx][j]);
            }
            fprintf(ctx.output, "\n");
            free(ctx.tail_buffer[idx]);
        }
        free(ctx.tail_buffer);
        free(ctx.tail_field_counts);
    }

    cisv_parser_destroy(parser);

    if (ctx.select_cols) {
        free(ctx.select_cols);
    }

    if (ctx.output != stdout) {
        fclose(ctx.output);
    }

    free(ctx.current_row);

    return 0;
}

#endif // CISV_CLI
