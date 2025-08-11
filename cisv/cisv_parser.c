#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
// NOTE: not dealing with windows for now, too much issues
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include "cisv_parser.h"

#define RINGBUF_SIZE (1 << 20) // 1 MiB (we may adjust according to needs)
// #define RINGBUF_SIZE (1 << 16) // 64kb (for memory safe reasons)
#define PREFETCH_DISTANCE 256

struct cisv_parser {
    uint8_t *base;              // pointer to the whole input, if memory-mapped
    size_t size;                // length of that mapping
    int fd;                     // the underlying file descriptor (-1 ⇒ none)
    uint8_t *ring;              // malloc’ed circular buffer when not mmapped
    size_t head;                // write head: next byte slot to fill
    uint8_t st;
    cisv_field_cb fcb;           // field callback fired whenever a full cell is ready
                                 // (delimiter or row-ending newline encountered, consistent with RFC 4180 rules)

    cisv_row_cb rcb;             // row callback fired after the last field of each record
    void *user;
    const uint8_t *field_start;  // where the in-progress field began
};
// State constants for branchless operations
#define S_UNQUOTED  0
#define S_QUOTED    1
#define S_QUOTE_ESC 2

// Action flags for each character in each state
#define ACT_NONE       0
#define ACT_FIELD      1
#define ACT_ROW        2
#define ACT_REPROCESS  4

// Lookup tables for branchless state transitions - initialized at compile time
static uint8_t state_table[3][256];
static uint8_t action_table[3][256];
static int tables_initialized = 0;

// Initialize lookup tables (called lazily)
static void init_tables(void) {
    if (tables_initialized) return;

    // Initialize state transitions
    for (int c = 0; c < 256; c++) {
        // S_UNQUOTED transitions
        state_table[S_UNQUOTED][c] = S_UNQUOTED;
        if (c == '"') state_table[S_UNQUOTED][c] = S_QUOTED;

        // S_QUOTED transitions
        state_table[S_QUOTED][c] = S_QUOTED;
        if (c == '"') state_table[S_QUOTED][c] = S_QUOTE_ESC;

        // S_QUOTE_ESC transitions
        if (c == '"') {
            state_table[S_QUOTE_ESC][c] = S_QUOTED;
        } else {
            state_table[S_QUOTE_ESC][c] = S_UNQUOTED;
        }
    }

    // Initialize action table
    memset(action_table, ACT_NONE, sizeof(action_table));

    // S_UNQUOTED actions
    action_table[S_UNQUOTED][','] = ACT_FIELD;
    action_table[S_UNQUOTED]['\n'] = ACT_FIELD | ACT_ROW;

    // S_QUOTE_ESC actions
    for (int c = 0; c < 256; c++) {
        if (c != '"') {
            action_table[S_QUOTE_ESC][c] = ACT_REPROCESS;
        }
    }

    tables_initialized = 1;
}

static inline void yield_field(cisv_parser *parser, const uint8_t *start, const uint8_t *end) {
    // Branchless check: multiply callback by validity flag
    size_t valid = (parser->fcb != NULL) & (start != NULL) & (end != NULL) & (end >= start);
    if (valid) {
        parser->fcb(parser->user, (const char *)start, (size_t)(end - start));
    }
}

static inline void yield_row(cisv_parser *parser) {
    if (parser->rcb) {
        parser->rcb(parser->user);
    }
}

#if defined(cisv_HAVE_AVX512) || defined(cisv_HAVE_AVX2)

static void parse_simd_chunk(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    // Ensure tables are initialized
    if (!tables_initialized) init_tables();

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // SIMD constants - create them on stack to avoid segfault
    uint8_t comma_bytes[64];
    uint8_t quote_bytes[64];
    uint8_t newline_bytes[64];
    memset(comma_bytes, ',', 64);
    memset(quote_bytes, '"', 64);
    memset(newline_bytes, '\n', 64);

    const cisv_vec comma_vec = cisv_LOAD(comma_bytes);
    const cisv_vec quote_vec = cisv_LOAD(quote_bytes);
    const cisv_vec newline_vec = cisv_LOAD(newline_bytes);

    while (cur + cisv_VEC_BYTES <= end) {
        // Prefetch next chunk
        __builtin_prefetch(cur + PREFETCH_DISTANCE, 0, 1);

        // Fast path for unquoted state
        if (parser->st == S_UNQUOTED) {
            cisv_vec chunk = cisv_LOAD(cur);

            #ifdef cisv_HAVE_AVX512
                uint64_t comma_mask = cisv_CMP_EQ(chunk, comma_vec);
                uint64_t quote_mask = cisv_CMP_EQ(chunk, quote_vec);
                uint64_t newline_mask = cisv_CMP_EQ(chunk, newline_vec);
                uint64_t combined = comma_mask | quote_mask | newline_mask;
            #else
                cisv_vec comma_cmp = cisv_CMP_EQ(chunk, comma_vec);
                cisv_vec quote_cmp = cisv_CMP_EQ(chunk, quote_vec);
                cisv_vec newline_cmp = cisv_CMP_EQ(chunk, newline_vec);
                cisv_vec combined_vec = cisv_OR_MASK(cisv_OR_MASK(comma_cmp, quote_cmp), newline_cmp);
                uint32_t combined = cisv_MOVEMASK(combined_vec);
            #endif

            if (!combined) {
                // No special chars, skip entire vector
                cur += cisv_VEC_BYTES;
                continue;
            }

            // Process special characters
            while (combined) {
                size_t pos = cisv_CTZ(combined);
                const uint8_t *special_pos = cur + pos;
                uint8_t c = *special_pos;

                // Branchless field/row handling
                uint8_t is_comma = (c == ',');
                uint8_t is_newline = (c == '\n');
                uint8_t is_quote = (c == '"');

                // Process field before special char
                if (special_pos > parser->field_start && (is_comma | is_newline)) {
                    yield_field(parser, parser->field_start, special_pos);
                    parser->field_start = special_pos + 1;
                }

                // Handle newline
                if (is_newline) {
                    yield_row(parser);
                }

                // Update state branchlessly
                parser->st = (parser->st & ~is_quote) | (S_QUOTED & -is_quote);

                // Clear processed bit
                combined &= combined - 1;
            }

            cur += cisv_VEC_BYTES;
        } else {
            // In quoted state - need scalar processing
            break;
        }
    }

    // Handle remainder with scalar code
    while (cur < end) {
        uint8_t c = *cur;
        uint8_t next_state = state_table[parser->st][c];
        uint8_t action = action_table[parser->st][c];

        // Branchless state update
        parser->st = next_state;

        // Handle actions branchlessly where possible
        if (action & ACT_FIELD) {
            yield_field(parser, parser->field_start, cur);
            parser->field_start = cur + 1;
        }
        if (action & ACT_ROW) {
            yield_row(parser);
        }

        // Reprocess requires going back
        cur += 1 - ((action & ACT_REPROCESS) >> 2);
    }
}

#elif defined(HAS_NEON)

// ARM NEON optimized parsing
static void parse_simd_chunk(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    // Ensure tables are initialized
    if (!tables_initialized) init_tables();

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // NEON constants
    uint8x16_t comma_vec = vdupq_n_u8(',');
    uint8x16_t quote_vec = vdupq_n_u8('"');
    uint8x16_t newline_vec = vdupq_n_u8('\n');

    while (cur + 16 <= end && parser->st == S_UNQUOTED) {
        // Prefetch next chunk
        __builtin_prefetch(cur + 64, 0, 1);

        // Load 16 bytes
        uint8x16_t chunk = vld1q_u8(cur);

        // Compare with special characters
        uint8x16_t comma_cmp = vceqq_u8(chunk, comma_vec);
        uint8x16_t quote_cmp = vceqq_u8(chunk, quote_vec);
        uint8x16_t newline_cmp = vceqq_u8(chunk, newline_vec);

        // Combine masks
        uint8x16_t combined = vorrq_u8(vorrq_u8(comma_cmp, quote_cmp), newline_cmp);

        // Check if any special char found
        uint64_t mask_low = vgetq_lane_u64(vreinterpretq_u64_u8(combined), 0);
        uint64_t mask_high = vgetq_lane_u64(vreinterpretq_u64_u8(combined), 1);

        if (!(mask_low | mask_high)) {
            // No special chars, advance
            cur += 16;
            continue;
        }

        // Process special characters byte by byte
        for (int i = 0; i < 16 && cur + i < end; i++) {
            uint8_t c = cur[i];
            if (c == ',' || c == '\n' || c == '"') {
                if (c == ',' || c == '\n') {
                    if (cur + i > parser->field_start) {
                        yield_field(parser, parser->field_start, cur + i);
                        parser->field_start = cur + i + 1;
                    }
                    if (c == '\n') {
                        yield_row(parser);
                    }
                } else if (c == '"') {
                    parser->st = S_QUOTED;
                    cur += i + 1;
                    goto scalar_fallback;
                }
            }
        }
        cur += 16;
    }

scalar_fallback:
    // Handle remainder with scalar code
    while (cur < end) {
        uint8_t c = *cur;
        uint8_t next_state = state_table[parser->st][c];
        uint8_t action = action_table[parser->st][c];

        parser->st = next_state;

        if (action & ACT_FIELD) {
            yield_field(parser, parser->field_start, cur);
            parser->field_start = cur + 1;
        }
        if (action & ACT_ROW) {
            yield_row(parser);
        }

        cur += 1 - ((action & ACT_REPROCESS) >> 2);
    }
}

#else
// Non-SIMD fallback with branchless optimizations
static void parse_simd_chunk(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    // Ensure tables are initialized
    if (!tables_initialized) init_tables();

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // Unroll loop by 8 for better performance
    while (cur + 8 <= end) {
        __builtin_prefetch(cur + 64, 0, 1);

        for (int i = 0; i < 8; i++) {
            uint8_t c = cur[i];
            uint8_t next_state = state_table[parser->st][c];
            uint8_t action = action_table[parser->st][c];

            parser->st = next_state;

            if (action & ACT_FIELD) {
                yield_field(parser, parser->field_start, cur + i);
                parser->field_start = cur + i + 1;
            }
            if (action & ACT_ROW) {
                yield_row(parser);
            }
            if (action & ACT_REPROCESS) {
                i--;
            }
        }
        cur += 8;
    }

    // Handle remainder
    while (cur < end) {
        uint8_t c = *cur;
        uint8_t next_state = state_table[parser->st][c];
        uint8_t action = action_table[parser->st][c];

        parser->st = next_state;

        if (action & ACT_FIELD) {
            yield_field(parser, parser->field_start, cur);
            parser->field_start = cur + 1;
        }
        if (action & ACT_ROW) {
            yield_row(parser);
        }

        cur += 1 - ((action & ACT_REPROCESS) >> 2);
    }
}
#endif

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->st = S_UNQUOTED;

    parse_simd_chunk(parser, buffer, len);

    // Handle final field if needed
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

    // Use madvise for sequential access
    madvise(base, st.st_size, MADV_SEQUENTIAL);

    size_t count = 0;

#if defined(cisv_HAVE_AVX512) || defined(cisv_HAVE_AVX2)
    // x86 SIMD path
    uint8_t newline_bytes[64];
    memset(newline_bytes, '\n', 64);
    const cisv_vec newline_vec = cisv_LOAD(newline_bytes);

    size_t i = 0;
    for (; i + cisv_VEC_BYTES <= (size_t)st.st_size; i += cisv_VEC_BYTES) {
        cisv_vec chunk = cisv_LOAD(base + i);
        #ifdef cisv_HAVE_AVX512
            uint64_t mask = cisv_CMP_EQ(chunk, newline_vec);
            count += __builtin_popcountll(mask);
        #else
            uint32_t mask = cisv_MOVEMASK(cisv_CMP_EQ(chunk, newline_vec));
            count += __builtin_popcount(mask);
        #endif
    }

    // Handle remainder
    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#elif defined(HAS_NEON)
    // ARM NEON path
    uint8x16_t newline_vec = vdupq_n_u8('\n');
    size_t i = 0;

    // Process 64 bytes at a time using 4 NEON registers
    for (; i + 64 <= (size_t)st.st_size; i += 64) {
        uint8x16_t data0 = vld1q_u8(base + i);
        uint8x16_t data1 = vld1q_u8(base + i + 16);
        uint8x16_t data2 = vld1q_u8(base + i + 32);
        uint8x16_t data3 = vld1q_u8(base + i + 48);

        uint8x16_t cmp0 = vceqq_u8(data0, newline_vec);
        uint8x16_t cmp1 = vceqq_u8(data1, newline_vec);
        uint8x16_t cmp2 = vceqq_u8(data2, newline_vec);
        uint8x16_t cmp3 = vceqq_u8(data3, newline_vec);

        // Count set bits in comparison results
        count += vaddvq_u8(vandq_u8(cmp0, vdupq_n_u8(1)));
        count += vaddvq_u8(vandq_u8(cmp1, vdupq_n_u8(1)));
        count += vaddvq_u8(vandq_u8(cmp2, vdupq_n_u8(1)));
        count += vaddvq_u8(vandq_u8(cmp3, vdupq_n_u8(1)));
    }

    // Process 16 bytes at a time
    for (; i + 16 <= (size_t)st.st_size; i += 16) {
        uint8x16_t data = vld1q_u8(base + i);
        uint8x16_t cmp = vceqq_u8(data, newline_vec);
        count += vaddvq_u8(vandq_u8(cmp, vdupq_n_u8(1)));
    }

    // Handle remainder
    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#else
    // Unrolled scalar version
    size_t i = 0;
    for (; i + 8 <= (size_t)st.st_size; i += 8) {
        count += (base[i] == '\n');
        count += (base[i+1] == '\n');
        count += (base[i+2] == '\n');
        count += (base[i+3] == '\n');
        count += (base[i+4] == '\n');
        count += (base[i+5] == '\n');
        count += (base[i+6] == '\n');
        count += (base[i+7] == '\n');
    }
    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#endif

    munmap(base, st.st_size);
    close(fd);
    return count;
}

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user) {
    cisv_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    // Align ring buffer to cache line
    if (posix_memalign((void**)&parser->ring, 64, RINGBUF_SIZE + 64) != 0) {
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

    // Platform-specific file hints
    #ifdef HAS_POSIX_FADVISE
        posix_fadvise(parser->fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    #elif defined(__APPLE__) && defined(HAS_RDADVISE)
        struct radvisory radv;
        radv.ra_offset = 0;
        radv.ra_count = 0;  // 0 means whole file
        fcntl(parser->fd, F_RDADVISE, &radv);
    #endif

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

    // mmap with platform-specific flags
    int mmap_flags = MAP_PRIVATE;
    #ifdef HAS_MAP_POPULATE
        mmap_flags |= MAP_POPULATE;
    #endif

    parser->base = mmap(NULL, parser->size, PROT_READ, mmap_flags, parser->fd, 0);
    if (parser->base == MAP_FAILED) {
        int err = errno;
        close(parser->fd);
        parser->fd = -1;
        return -err;
    }

    // Advise kernel about access pattern
    madvise(parser->base, parser->size, MADV_SEQUENTIAL);
    #ifdef MADV_WILLNEED
        madvise(parser->base, parser->size < (1<<20) ? parser->size : (1<<20), MADV_WILLNEED);
    #endif

    return parse_memory(parser, parser->base, parser->size);
}

int cisv_parser_write(cisv_parser *parser, const uint8_t *chunk, size_t len) {
    if (!parser || !chunk || len >= RINGBUF_SIZE) return -EINVAL;

    // Branchless overflow handling
    size_t overflow = (parser->head + len > RINGBUF_SIZE);
    if (overflow) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }

    memcpy(parser->ring + parser->head, chunk, len);
    parser->head += len;

    // Check for newline or buffer threshold
    uint8_t has_newline = (memchr(chunk, '\n', len) != NULL);
    uint8_t threshold = (parser->head > (RINGBUF_SIZE / 2));
    if (has_newline | threshold) {
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
                printf("cisv version v0.0.1-rc10\n");
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
