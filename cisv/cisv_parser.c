#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
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
    uint8_t *ring;              // malloc'ed circular buffer when not mmapped
    size_t head;                // write head: next byte slot to fill
    uint8_t st;                 // parser state

    // Configuration options
    char delimiter;             // field delimiter character (default ',')
    char quote;                 // quote character (default '"')
    char escape;                // escape character (0 means use RFC4180-style "" escaping)
    bool skip_empty_lines;      // whether to skip empty lines
    char comment;               // comment character (0 means no comments)
    bool trim;                  // whether to trim whitespace from fields
    bool relaxed;               // whether to use relaxed parsing rules
    size_t max_row_size;        // maximum allowed row size (0 = unlimited)
    int from_line;              // start parsing from this line number (1-based)
    int to_line;                // stop parsing at this line number (0 = until end)
    bool skip_lines_with_error; // whether to skip lines that cause errors

    // State tracking
    int current_line;           // current line number being processed
    bool in_comment;            // whether we're currently in a comment
    int tables_initialized;     // whether lookup tables are initialized

    // Callbacks
    cisv_field_cb fcb;          // field callback fired whenever a full cell is ready
                                // (delimiter or row-ending newline encountered, consistent with RFC 4180 rules)
    cisv_row_cb rcb;            // row callback fired after the last field of each record
    cisv_error_cb ecb;          // error callback for handling parse errors
    void *user;                 // user data for callbacks

    const uint8_t *field_start; // where the in-progress field began
    const uint8_t *row_start;   // where the current row began
    size_t current_row_size;    // size of current row being processed

    // Dynamic lookup tables for current configuration
    uint8_t *state_table;       // state transition table
    uint8_t *action_table;      // action table
};

// State constants for branchless operations
#define S_UNQUOTED  0
#define S_QUOTED    1
#define S_QUOTE_ESC 2
#define S_COMMENT   3

// Action flags for each character in each state
#define ACT_NONE       0
#define ACT_FIELD      1
#define ACT_ROW        2
#define ACT_REPROCESS  4
#define ACT_SKIP       8

// Initialize configuration with defaults
void cisv_config_init(cisv_config *config) {
    memset(config, 0, sizeof(*config));
    config->delimiter = ',';
    config->quote = '"';
    config->escape = 0;  // Use RFC4180-style "" escaping by default
    config->skip_empty_lines = false;
    config->comment = 0;  // No comments by default
    config->trim = false;
    config->relaxed = false;
    config->max_row_size = 0;  // Unlimited
    config->from_line = 1;
    config->to_line = 0;  // Until end
    config->skip_lines_with_error = false;
}

// Initialize lookup tables for the parser's configuration
static void init_tables(cisv_parser *parser) {
    if (parser->tables_initialized) return;

    // Allocate tables if not already allocated
    if (!parser->state_table) {
        parser->state_table = calloc(4 * 256, sizeof(uint8_t));
        parser->action_table = calloc(4 * 256, sizeof(uint8_t));
        if (!parser->state_table || !parser->action_table) {
            return;  // Handle allocation failure gracefully
        }
    }

    // Get table pointers for easier access
    uint8_t (*state_table)[256] = (uint8_t (*)[256])parser->state_table;
    uint8_t (*action_table)[256] = (uint8_t (*)[256])parser->action_table;

    // Initialize state transitions
    for (int c = 0; c < 256; c++) {
        // S_UNQUOTED transitions
        state_table[S_UNQUOTED][c] = S_UNQUOTED;
        if (c == parser->quote) {
            state_table[S_UNQUOTED][c] = S_QUOTED;
        } else if (parser->comment && c == parser->comment) {
            state_table[S_UNQUOTED][c] = S_COMMENT;
        }

        // S_QUOTED transitions
        state_table[S_QUOTED][c] = S_QUOTED;
        if (parser->escape && c == parser->escape) {
            state_table[S_QUOTED][c] = S_QUOTE_ESC;
        } else if (c == parser->quote) {
            state_table[S_QUOTED][c] = S_QUOTE_ESC;
        }

        // S_QUOTE_ESC transitions
        if (parser->escape) {
            // With explicit escape character, always return to quoted state
            state_table[S_QUOTE_ESC][c] = S_QUOTED;
        } else {
            // RFC4180-style: "" becomes a literal quote
            if (c == parser->quote) {
                state_table[S_QUOTE_ESC][c] = S_QUOTED;
            } else {
                state_table[S_QUOTE_ESC][c] = S_UNQUOTED;
            }
        }

        // S_COMMENT transitions - stay in comment until newline
        state_table[S_COMMENT][c] = S_COMMENT;
        if (c == '\n') {
            state_table[S_COMMENT][c] = S_UNQUOTED;
        }
    }

    // Initialize action table
    memset(action_table, ACT_NONE, 4 * 256);

    // S_UNQUOTED actions
    action_table[S_UNQUOTED][(uint8_t)parser->delimiter] = ACT_FIELD;
    action_table[S_UNQUOTED]['\n'] = ACT_FIELD | ACT_ROW;
    action_table[S_UNQUOTED]['\r'] = ACT_FIELD;  // Handle CRLF

    // S_QUOTE_ESC actions
    if (!parser->escape) {
        // RFC4180-style: reprocess non-quote characters
        for (int c = 0; c < 256; c++) {
            if (c != parser->quote) {
                action_table[S_QUOTE_ESC][c] = ACT_REPROCESS;
            }
        }
    }

    // S_COMMENT actions - skip everything except newline
    for (int c = 0; c < 256; c++) {
        action_table[S_COMMENT][c] = ACT_SKIP;
    }
    action_table[S_COMMENT]['\n'] = ACT_ROW;

    parser->tables_initialized = 1;
}

static inline const uint8_t* trim_start(const uint8_t *start, const uint8_t *end) {
    while (start < end && isspace(*start)) start++;
    return start;
}

static inline const uint8_t* trim_end(const uint8_t *start, const uint8_t *end) {
    while (end > start && isspace(*(end - 1))) end--;
    return end;
}

static inline void yield_field(cisv_parser *parser, const uint8_t *start, const uint8_t *end) {
    // Apply trimming if configured
    if (parser->trim) {
        start = trim_start(start, end);
        end = trim_end(start, end);
    }

    // Branchless check: multiply callback by validity flag
    size_t valid = (parser->fcb != NULL) & (start != NULL) & (end != NULL) & (end >= start);
    if (valid) {
        parser->fcb(parser->user, (const char *)start, (size_t)(end - start));
    }
}

static inline void yield_row(cisv_parser *parser) {
    // Check if we should skip empty lines
    if (parser->skip_empty_lines && parser->field_start == parser->row_start) {
        parser->row_start = parser->field_start;
        return;
    }

    // Check line range
    if (parser->current_line < parser->from_line) {
        parser->current_line++;
        parser->row_start = parser->field_start;
        return;
    }

    if (parser->to_line > 0 && parser->current_line > parser->to_line) {
        return;
    }

    if (parser->rcb) {
        parser->rcb(parser->user);
    }

    parser->current_line++;
    parser->row_start = parser->field_start;
    parser->current_row_size = 0;
}

static inline void handle_error(cisv_parser *parser, const char *msg) {
    if (parser->ecb) {
        parser->ecb(parser->user, parser->current_line, msg);
    }

    if (!parser->skip_lines_with_error) {
        // Could set an error flag here to stop parsing
    }
}

// SIMD implementations with configuration support
#if defined(cisv_HAVE_AVX512) || defined(cisv_HAVE_AVX2)

static void parse_simd_chunk(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    // Ensure tables are initialized
    if (!parser->tables_initialized) init_tables(parser);

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // Get table pointers
    uint8_t (*state_table)[256] = (uint8_t (*)[256])parser->state_table;
    uint8_t (*action_table)[256] = (uint8_t (*)[256])parser->action_table;

    // SIMD constants - create them on stack to avoid segfault
    uint8_t delim_bytes[64];
    uint8_t quote_bytes[64];
    uint8_t newline_bytes[64];
    uint8_t comment_bytes[64];

    memset(delim_bytes, parser->delimiter, 64);
    memset(quote_bytes, parser->quote, 64);
    memset(newline_bytes, '\n', 64);
    if (parser->comment) {
        memset(comment_bytes, parser->comment, 64);
    }

    const cisv_vec delim_vec = cisv_LOAD(delim_bytes);
    const cisv_vec quote_vec = cisv_LOAD(quote_bytes);
    const cisv_vec newline_vec = cisv_LOAD(newline_bytes);
    const cisv_vec comment_vec = parser->comment ? cisv_LOAD(comment_bytes) : delim_vec;

    while (cur + cisv_VEC_BYTES <= end) {
        // Check max row size if configured
        if (parser->max_row_size > 0) {
            parser->current_row_size += cisv_VEC_BYTES;
            if (parser->current_row_size > parser->max_row_size) {
                handle_error(parser, "Row exceeds maximum size");
                // Skip to next line
                while (cur < end && *cur != '\n') cur++;
                if (cur < end) cur++;
                parser->current_row_size = 0;
                parser->row_start = cur;
                parser->field_start = cur;
                continue;
            }
        }

        // Prefetch next chunk
        __builtin_prefetch(cur + PREFETCH_DISTANCE, 0, 1);

        // Fast path for unquoted state
        if (parser->st == S_UNQUOTED && !parser->in_comment) {
            cisv_vec chunk = cisv_LOAD(cur);

            #ifdef cisv_HAVE_AVX512
                uint64_t delim_mask = cisv_CMP_EQ(chunk, delim_vec);
                uint64_t quote_mask = cisv_CMP_EQ(chunk, quote_vec);
                uint64_t newline_mask = cisv_CMP_EQ(chunk, newline_vec);
                uint64_t comment_mask = parser->comment ? cisv_CMP_EQ(chunk, comment_vec) : 0;
                uint64_t combined = delim_mask | quote_mask | newline_mask | comment_mask;
            #else
                cisv_vec delim_cmp = cisv_CMP_EQ(chunk, delim_vec);
                cisv_vec quote_cmp = cisv_CMP_EQ(chunk, quote_vec);
                cisv_vec newline_cmp = cisv_CMP_EQ(chunk, newline_vec);
                cisv_vec combined_vec = cisv_OR_MASK(cisv_OR_MASK(delim_cmp, quote_cmp), newline_cmp);
                if (parser->comment) {
                    cisv_vec comment_cmp = cisv_CMP_EQ(chunk, comment_vec);
                    combined_vec = cisv_OR_MASK(combined_vec, comment_cmp);
                }
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
                uint8_t is_delim = (c == parser->delimiter);
                uint8_t is_newline = (c == '\n');
                uint8_t is_quote = (c == parser->quote);
                uint8_t is_comment = parser->comment && (c == parser->comment);

                // Process field before special char
                if (special_pos > parser->field_start && (is_delim | is_newline)) {
                    yield_field(parser, parser->field_start, special_pos);
                    parser->field_start = special_pos + 1;
                }

                // Handle newline
                if (is_newline) {
                    yield_row(parser);
                }

                // Update state branchlessly
                parser->st = (parser->st & ~is_quote) | (S_QUOTED & -is_quote);
                parser->in_comment = parser->in_comment | is_comment;

                // Clear processed bit
                combined &= combined - 1;
            }

            cur += cisv_VEC_BYTES;
        } else {
            // In quoted/comment state - need scalar processing
            break;
        }
    }

    // Handle remainder with scalar code
    while (cur < end) {
        uint8_t c = *cur;

        // Handle comment state
        if (parser->in_comment) {
            if (c == '\n') {
                parser->in_comment = false;
                yield_row(parser);
            }
            cur++;
            continue;
        }

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
    if (!parser->tables_initialized) init_tables(parser);

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // Get table pointers
    uint8_t (*state_table)[256] = (uint8_t (*)[256])parser->state_table;
    uint8_t (*action_table)[256] = (uint8_t (*)[256])parser->action_table;

    // NEON constants
    uint8x16_t delim_vec = vdupq_n_u8(parser->delimiter);
    uint8x16_t quote_vec = vdupq_n_u8(parser->quote);
    uint8x16_t newline_vec = vdupq_n_u8('\n');
    uint8x16_t comment_vec = parser->comment ? vdupq_n_u8(parser->comment) : delim_vec;

    while (cur + 16 <= end && parser->st == S_UNQUOTED && !parser->in_comment) {
        // Check max row size if configured
        if (parser->max_row_size > 0) {
            parser->current_row_size += 16;
            if (parser->current_row_size > parser->max_row_size) {
                handle_error(parser, "Row exceeds maximum size");
                // Skip to next line
                while (cur < end && *cur != '\n') cur++;
                if (cur < end) cur++;
                parser->current_row_size = 0;
                parser->row_start = cur;
                parser->field_start = cur;
                continue;
            }
        }

        // Prefetch next chunk
        __builtin_prefetch(cur + 64, 0, 1);

        // Load 16 bytes
        uint8x16_t chunk = vld1q_u8(cur);

        // Compare with special characters
        uint8x16_t delim_cmp = vceqq_u8(chunk, delim_vec);
        uint8x16_t quote_cmp = vceqq_u8(chunk, quote_vec);
        uint8x16_t newline_cmp = vceqq_u8(chunk, newline_vec);

        // Combine masks
        uint8x16_t combined = vorrq_u8(vorrq_u8(delim_cmp, quote_cmp), newline_cmp);
        if (parser->comment) {
            uint8x16_t comment_cmp = vceqq_u8(chunk, comment_vec);
            combined = vorrq_u8(combined, comment_cmp);
        }

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
            if (c == parser->delimiter || c == '\n' || c == parser->quote ||
                (parser->comment && c == parser->comment)) {
                if (c == parser->delimiter || c == '\n') {
                    if (cur + i > parser->field_start) {
                        yield_field(parser, parser->field_start, cur + i);
                        parser->field_start = cur + i + 1;
                    }
                    if (c == '\n') {
                        yield_row(parser);
                    }
                } else if (c == parser->quote) {
                    parser->st = S_QUOTED;
                    cur += i + 1;
                    goto scalar_fallback;
                } else if (parser->comment && c == parser->comment) {
                    parser->in_comment = true;
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

        // Handle comment state
        if (parser->in_comment) {
            if (c == '\n') {
                parser->in_comment = false;
                yield_row(parser);
            }
            cur++;
            continue;
        }

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
    if (!parser->tables_initialized) init_tables(parser);

    const uint8_t *cur = buffer;
    const uint8_t *end = buffer + len;

    // Get table pointers
    uint8_t (*state_table)[256] = (uint8_t (*)[256])parser->state_table;
    uint8_t (*action_table)[256] = (uint8_t (*)[256])parser->action_table;

    // Unroll loop by 8 for better performance
    while (cur + 8 <= end) {
        // Check max row size if configured
        if (parser->max_row_size > 0 && parser->current_row_size > parser->max_row_size) {
            handle_error(parser, "Row exceeds maximum size");
            // Skip to next line
            while (cur < end && *cur != '\n') cur++;
            if (cur < end) {
                cur++;
                parser->current_row_size = 0;
                parser->row_start = cur;
                parser->field_start = cur;
            }
            continue;
        }

        __builtin_prefetch(cur + 64, 0, 1);

        for (int i = 0; i < 8; i++) {
            uint8_t c = cur[i];

            // Handle comment state
            if (parser->in_comment) {
                if (c == '\n') {
                    parser->in_comment = false;
                    yield_row(parser);
                }
                continue;
            }

            // Check for comment start
            if (parser->comment && parser->st == S_UNQUOTED && c == parser->comment) {
                parser->in_comment = true;
                continue;
            }

            uint8_t next_state = state_table[parser->st][c];
            uint8_t action = action_table[parser->st][c];

            parser->st = next_state;
            parser->current_row_size++;

            if (action & ACT_FIELD) {
                yield_field(parser, parser->field_start, cur + i);
                parser->field_start = cur + i + 1;
            }
            if (action & ACT_ROW) {
                yield_row(parser);
                parser->current_row_size = 0;
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

        // Handle comment state
        if (parser->in_comment) {
            if (c == '\n') {
                parser->in_comment = false;
                yield_row(parser);
                parser->current_row_size = 0;
            }
            cur++;
            continue;
        }

        // Check for comment start
        if (parser->comment && parser->st == S_UNQUOTED && c == parser->comment) {
            parser->in_comment = true;
            cur++;
            continue;
        }

        // Check max row size
        if (parser->max_row_size > 0 && ++parser->current_row_size > parser->max_row_size) {
            handle_error(parser, "Row exceeds maximum size");
            // Skip to next line
            while (cur < end && *cur != '\n') cur++;
            if (cur < end) {
                cur++;
                parser->current_row_size = 0;
                parser->row_start = cur;
                parser->field_start = cur;
            }
            continue;
        }

        uint8_t next_state = state_table[parser->st][c];
        uint8_t action = action_table[parser->st][c];

        parser->st = next_state;

        if (action & ACT_FIELD) {
            yield_field(parser, parser->field_start, cur);
            parser->field_start = cur + 1;
        }
        if (action & ACT_ROW) {
            yield_row(parser);
            parser->current_row_size = 0;
        }

        cur += 1 - ((action & ACT_REPROCESS) >> 2);
    }
}

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->row_start = buffer;
    parser->st = S_UNQUOTED;
    parser->in_comment = false;
    parser->current_row_size = 0;

    parse_simd_chunk(parser, buffer, len);

    // Handle final field if not terminated
    if (parser->field_start < buffer + len) {
        yield_field(parser, parser->field_start, buffer + len);
        parser->field_start = buffer + len;
    }

    // Handle final row if not terminated by newline
    if (parser->row_start < buffer + len) {
        yield_row(parser);
    }

    return 0;
}

cisv_parser *cisv_parser_create_with_config(const cisv_config *config) {
    cisv_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    // Copy configuration
    parser->delimiter = config->delimiter;
    parser->quote = config->quote;
    parser->escape = config->escape;
    parser->skip_empty_lines = config->skip_empty_lines;
    parser->comment = config->comment;
    parser->trim = config->trim;
    parser->relaxed = config->relaxed;
    parser->max_row_size = config->max_row_size;
    parser->from_line = config->from_line;
    parser->to_line = config->to_line;
    parser->skip_lines_with_error = config->skip_lines_with_error;

    parser->fcb = config->field_cb;
    parser->rcb = config->row_cb;
    parser->ecb = config->error_cb;
    parser->user = config->user;

    parser->current_line = 1;
    parser->in_comment = false;
    parser->tables_initialized = 0;

    // Allocate ring buffer for streaming
    if (posix_memalign((void**)&parser->ring, 64, RINGBUF_SIZE + 64) != 0) {
        free(parser);
        return NULL;
    }

    parser->fd = -1;
    return parser;
}

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user) {
    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = fcb;
    config.row_cb = rcb;
    config.user = user;
    return cisv_parser_create_with_config(&config);
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
    free(parser->state_table);
    free(parser->action_table);
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

size_t cisv_parser_count_rows(const char *path) {
    cisv_config config;
    cisv_config_init(&config);
    return cisv_parser_count_rows_with_config(path, &config);
}

size_t cisv_parser_count_rows_with_config(const char *path, const cisv_config *config) {
    size_t count = 0;
    cisv_config local_config = *config;
    local_config.field_cb = NULL;
    local_config.row_cb = [](void *user) { (*((size_t*)user))++; };
    local_config.user = &count;
    local_config.error_cb = NULL;

    cisv_parser *parser = cisv_parser_create_with_config(&local_config);
    if (!parser) return 0;

    int ret = cisv_parser_parse_file(parser, path);
    cisv_parser_destroy(parser);

    return (ret < 0) ? 0 : count;
}

int cisv_parser_write(cisv_parser *parser, const uint8_t *chunk, size_t len) {
    if (!parser || !chunk || len >= RINGBUF_SIZE) return -EINVAL;

    // Handle buffer overflow
    if (parser->head + len > RINGBUF_SIZE) {
        parse_memory(parser, parser->ring, parser->head);
        parser->head = 0;
    }

    memcpy(parser->ring + parser->head, chunk, len);
    parser->head += len;

    // Parse if buffer reaches threshold or contains newline
    if (parser->head > (RINGBUF_SIZE / 2) || memchr(chunk, '\n', len)) {
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

int cisv_parser_get_line_number(const cisv_parser *parser) {
    return parser ? parser->current_line : 0;
}
