#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
// NOTE: not dealing with windows for now, too much issues
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include "cisv_parser.h"

#ifdef __AVX512F__
#include <immintrin.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

#define RINGBUF_SIZE (1 << 20) // 1 MiB
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

    // Allocate both tables in one allocation for better cache locality
    if (!parser->state_table) {
        parser->state_table = aligned_alloc(64, 8 * 256);  // Align to cache line
        if (!parser->state_table) return;
        parser->action_table = parser->state_table + (4 * 256);
        memset(parser->state_table, 0, 8 * 256);
    }

    uint8_t (*st)[256] = (uint8_t (*)[256])parser->state_table;
    uint8_t (*at)[256] = (uint8_t (*)[256])parser->action_table;

    // Unroll initialization loops for better performance
    // Pre-calculate commonly used values
    const uint8_t q = parser->quote;
    const uint8_t d = parser->delimiter;
    const uint8_t e = parser->escape;
    const uint8_t c = parser->comment;

    // Initialize with SIMD where possible
    #ifdef __AVX2__
    __m256i unquoted_state = _mm256_set1_epi8(S_UNQUOTED);
    __m256i quoted_state = _mm256_set1_epi8(S_QUOTED);
    __m256i comment_state = _mm256_set1_epi8(S_COMMENT);

    for (int i = 0; i < 256; i += 32) {
        _mm256_store_si256((__m256i*)&st[S_UNQUOTED][i], unquoted_state);
        _mm256_store_si256((__m256i*)&st[S_QUOTED][i], quoted_state);
        _mm256_store_si256((__m256i*)&st[S_COMMENT][i], comment_state);
    }
    #else
    memset(st[S_UNQUOTED], S_UNQUOTED, 256);
    memset(st[S_QUOTED], S_QUOTED, 256);
    memset(st[S_COMMENT], S_COMMENT, 256);
    #endif

    // Set special transitions
    st[S_UNQUOTED][q] = S_QUOTED;
    if (c) st[S_UNQUOTED][c] = S_COMMENT;

    if (e) {
        st[S_QUOTED][e] = S_QUOTE_ESC;
        memset(st[S_QUOTE_ESC], S_QUOTED, 256);
    } else {
        st[S_QUOTED][q] = S_QUOTE_ESC;
        memset(st[S_QUOTE_ESC], S_UNQUOTED, 256);
        st[S_QUOTE_ESC][q] = S_QUOTED;
    }

    st[S_COMMENT]['\n'] = S_UNQUOTED;

    // Initialize actions with minimal branches
    memset(at, ACT_NONE, 4 * 256);
    at[S_UNQUOTED][d] = ACT_FIELD;
    at[S_UNQUOTED]['\n'] = ACT_FIELD | ACT_ROW;
    at[S_UNQUOTED]['\r'] = ACT_FIELD;

    if (!e) {
        // Vectorize the action table initialization
        for (int i = 0; i < 256; i++) {
            at[S_QUOTE_ESC][i] = (i != q) ? ACT_REPROCESS : ACT_NONE;
        }
    }

    // Use SIMD for comment actions
    #ifdef __AVX2__
    __m256i skip_act = _mm256_set1_epi8(ACT_SKIP);
    for (int i = 0; i < 256; i += 32) {
        _mm256_store_si256((__m256i*)&at[S_COMMENT][i], skip_act);
    }
    #else
    memset(at[S_COMMENT], ACT_SKIP, 256);
    #endif
    at[S_COMMENT]['\n'] = ACT_ROW;

    parser->tables_initialized = 1;
}

// SIMD-optimized whitespace detection lookup table
// Ultra-fast trimming with AVX512/AVX2
static inline const uint8_t* trim_start(const uint8_t *start, const uint8_t *end) {
    size_t len = end - start;

#ifdef __AVX512F__
    if (len >= 64) {
        const __m512i max_ws = _mm512_set1_epi8(32);

        while (len >= 64) {
            __m512i chunk = _mm512_loadu_si512(start);
            __mmask64 is_ws = _mm512_cmple_epu8_mask(chunk, max_ws);

            if (is_ws != 0xFFFFFFFFFFFFFFFFULL) {
                return start + __builtin_ctzll(~is_ws);
            }
            start += 64;
            len -= 64;
        }
    }
#elif defined(__AVX2__)
    if (len >= 32) {
        const __m256i max_ws = _mm256_set1_epi8(32);

        while (len >= 32) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)start);
            __m256i cmp = _mm256_cmpgt_epi8(chunk, max_ws);
            uint32_t mask = _mm256_movemask_epi8(cmp);

            if (mask) {
                return start + __builtin_ctz(mask);
            }
            start += 32;
            len -= 32;
        }
    }
#endif

    // Unrolled 8-byte processing
    while (len >= 8) {
        uint64_t v = *(uint64_t*)start;
        uint64_t has_non_ws = ((v & 0xE0E0E0E0E0E0E0E0ULL) != 0) |
                              ((v & 0x1F1F1F1F1F1F1F1FULL) > 0x0D0D0D0D0D0D0D0DULL);
        if (has_non_ws) {
            for (int i = 0; i < 8; i++) {
                if ((uint8_t)(v >> (i*8)) > 32) return start + i;
            }
        }
        start += 8;
        len -= 8;
    }

    // 4-byte processing
    if (len >= 4) {
        uint32_t v = *(uint32_t*)start;
        for (int i = 0; i < 4; i++) {
            uint8_t c = (v >> (i*8)) & 0xFF;
            if (c > 32) return start + i;
        }
        start += 4;
        len -= 4;
    }

    // Remainder
    switch(len) {
        case 3: if (*start > 32) return start; start++;
                /* fallthrough */
        case 2: if (*start > 32) return start; start++;
                /* fallthrough */
        case 1: if (*start > 32) return start; start++;
    }

    return end;
}

static inline const uint8_t* trim_end(const uint8_t *start, const uint8_t *end) {
    size_t len = end - start;

#ifdef __AVX512F__
    while (len >= 64) {
        const uint8_t *check = end - 64;
        __m512i chunk = _mm512_loadu_si512(check);
        const __m512i max_ws = _mm512_set1_epi8(32);
        __mmask64 is_non_ws = _mm512_cmpgt_epu8_mask(chunk, max_ws);

        if (is_non_ws) {
            int last_non_ws = 63 - __builtin_clzll(is_non_ws);
            return check + last_non_ws + 1;
        }
        end -= 64;
        len -= 64;
    }
#elif defined(__AVX2__)
    while (len >= 32) {
        const uint8_t *check = end - 32;
        __m256i chunk = _mm256_loadu_si256((__m256i*)check);
        const __m256i max_ws = _mm256_set1_epi8(32);
        __m256i cmp = _mm256_cmpgt_epi8(chunk, max_ws);
        uint32_t mask = _mm256_movemask_epi8(cmp);

        if (mask) {
            int last_non_ws = 31 - __builtin_clz(mask);
            return check + last_non_ws + 1;
        }
        end -= 32;
        len -= 32;
    }
#endif

    // Unrolled 8-byte processing
    while (len >= 8) {
        const uint8_t *check = end - 8;
        uint64_t v = *(uint64_t*)check;

        for (int i = 7; i >= 0; i--) {
            if ((uint8_t)(v >> (i*8)) > 32) return check + i + 1;
        }
        end -= 8;
        len -= 8;
    }

    // 4-byte processing
    if (len >= 4) {
        const uint8_t *check = end - 4;
        uint32_t v = *(uint32_t*)check;
        for (int i = 3; i >= 0; i--) {
            if ((uint8_t)(v >> (i*8)) > 32) return check + i + 1;
        }
        end -= 4;
        len -= 4;
    }

    // Remainder
    while (len-- > 0) {
        if (*(--end) > 32) return end + 1;
    }

    return start;
}

// yield_field with prefetching and branchless code
static inline void yield_field(cisv_parser *parser, const uint8_t *start, const uint8_t *end) {
    // Prefetch parser structure for next access
    __builtin_prefetch(parser, 0, 3);

    // Branchless trimming using conditional move
    const uint8_t *s = start;
    const uint8_t *e = end;

    // Use conditional assignment instead of branch
    const uint8_t *trimmed_s = trim_start(s, e);
    const uint8_t *trimmed_e = trim_end(trimmed_s, e);

    // Branchless selection: if trim is 0, use original, if 1, use trimmed
    uintptr_t mask = -(uintptr_t)parser->trim;
    s = (const uint8_t*)(((uintptr_t)trimmed_s & mask) | ((uintptr_t)s & ~mask));
    e = (const uint8_t*)(((uintptr_t)trimmed_e & mask) | ((uintptr_t)e & ~mask));

    // Combine all conditions into single branch
    uintptr_t fcb_addr = (uintptr_t)parser->fcb;
    uintptr_t valid_mask = -(fcb_addr != 0);
    valid_mask &= -(s != 0);
    valid_mask &= -(e != 0);
    valid_mask &= -(e >= s);

    // Single branch for callback execution
    if (valid_mask) {
        // Prefetch user data for callback
        __builtin_prefetch(parser->user, 0, 1);
        parser->fcb(parser->user, (const char *)s, (size_t)(e - s));
    }
}

// yield_row with reduced branches
static inline void yield_row(cisv_parser *parser) {
    // Prefetch frequently accessed memory
    __builtin_prefetch(&parser->current_line, 1, 3);
    __builtin_prefetch(&parser->row_start, 1, 3);

    // Compute all conditions upfront
    int is_empty_line = (parser->field_start == parser->row_start);
    int skip_empty = parser->skip_empty_lines & is_empty_line;
    int before_range = (parser->current_line < parser->from_line);
    int after_range = (parser->to_line > 0) & (parser->current_line > parser->to_line);
    int in_range = !before_range & !after_range;

    // Branchless increment of current_line (always happens except when after range)
    parser->current_line += !after_range;

    // Branchless update of row_start (happens except when after range)
    uintptr_t new_row_start = (uintptr_t)parser->field_start;
    uintptr_t old_row_start = (uintptr_t)parser->row_start;
    parser->row_start = (uint8_t*)((old_row_start & -after_range) | (new_row_start & ~(-after_range)));

    // Branchless reset of row_size
    parser->current_row_size &= after_range;

    // Single branch for callback (most common case last for better prediction)
    if ((!skip_empty) & in_range & (parser->rcb != NULL)) {
        __builtin_prefetch(parser->user, 0, 1);
        parser->rcb(parser->user);
    }
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
#endif

static int parse_memory(cisv_parser *parser, const uint8_t *buffer, size_t len) {
    parser->field_start = buffer;
    parser->row_start = buffer;
    parser->st = S_UNQUOTED;
    parser->current_row_size = 0;
    parser->in_comment = false;

    parse_simd_chunk(parser, buffer, len);

    // Handle final field if needed
    if (parser->field_start < buffer + len && !parser->in_comment) {
        yield_field(parser, parser->field_start, buffer + len);
        // Yield final row if there's content
        if (parser->field_start > parser->row_start || !parser->skip_empty_lines) {
            yield_row(parser);
        }
    }
    return 0;
}

// Count rows with configuration support
size_t cisv_parser_count_rows_with_config(const char *path, const cisv_config *config) {
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
    int current_line = 1;
    bool in_quote = false;
    bool in_comment = false;

    // Handle configured line range and comments
    for (size_t i = 0; i < (size_t)st.st_size; i++) {
        uint8_t c = base[i];

        // Handle comments if configured
        if (config && config->comment && !in_quote) {
            if (c == config->comment) {
                in_comment = true;
            }
        }

        // Handle quotes if configured
        if (config && !in_comment) {
            if (c == config->quote) {
                in_quote = !in_quote;
            }
        }

        // Count newlines
        if (c == '\n') {
            if (!in_quote) {
                current_line++;
                if (!config ||
                    (current_line > config->from_line &&
                     (config->to_line == 0 || current_line <= config->to_line))) {
                    if (!in_comment || !config->skip_empty_lines) {
                        count++;
                    }
                }
                in_comment = false;
            }
        }
    }

    munmap(base, st.st_size);
    close(fd);
    return count;
}

// Legacy count function - uses default configuration
size_t cisv_parser_count_rows(const char *path) {
    cisv_config config;
    cisv_config_init(&config);
    return cisv_parser_count_rows_with_config(path, &config);
}

// Create parser with configuration
cisv_parser *cisv_parser_create_with_config(const cisv_config *config) {
    cisv_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) return NULL;

    // Align ring buffer to cache line
    if (posix_memalign((void**)&parser->ring, 64, RINGBUF_SIZE + 64) != 0) {
        free(parser);
        return NULL;
    }

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

    // Initialize callbacks
    parser->fcb = config->field_cb;
    parser->rcb = config->row_cb;
    parser->ecb = config->error_cb;
    parser->user = config->user;

    // Initialize state
    parser->fd = -1;
    parser->current_line = 1;
    parser->in_comment = false;
    parser->tables_initialized = 0;

    // Allocate and initialize lookup tables
    parser->state_table = calloc(4 * 256, sizeof(uint8_t));
    parser->action_table = calloc(4 * 256, sizeof(uint8_t));
    if (!parser->state_table || !parser->action_table) {
        free(parser->state_table);
        free(parser->action_table);
        free(parser->ring);
        free(parser);
        return NULL;
    }

    // Initialize tables for this configuration
    init_tables(parser);

    return parser;
}

// Legacy create function - uses default configuration
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

    // Reset line counter for new file
    parser->current_line = 1;
    parser->in_comment = false;

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

// Get current line number
int cisv_parser_get_line_number(const cisv_parser *parser) {
    return parser ? parser->current_line : 0;
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

    // Store configuration for output
    cisv_config *config;
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
    printf("\n----------\nExamples:\n");
    printf("  %s data.csv                    # Parse and display CSV\n", prog);
    printf("  %s -c data.csv                 # Count rows\n", prog);
    printf("  %s -d ';' -q '\\'' data.csv      # Use semicolon delimiter and single quote\n", prog);
    printf("  %s -t --skip-empty data.csv    # Trim fields and skip empty lines\n", prog);
    printf("  %s -m '#' data.csv             # Skip lines starting with #\n", prog);
    printf("  %s --from-line 10 --to-line 100 data.csv  # Parse lines 10-100\n", prog);
    printf("\nFor write options, use: %s write --help\n", prog);
}

static double get_time_ms() {
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

    // Initialize configuration
    cisv_config config;
    cisv_config_init(&config);

    // Initialize context
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
                config.delimiter = optarg[0];
                break;

            case 'q':
                config.quote = optarg[0];
                break;

            case 'e':
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

            case 1: // --skip-empty
                config.skip_empty_lines = true;
                break;

            case 2: // --skip-errors
                config.skip_lines_with_error = true;
                break;

            case 3: // --max-row
                config.max_row_size = atol(optarg);
                break;

            case 4: // --from-line
                config.from_line = atoi(optarg);
                break;

            case 5: // --to-line
                config.to_line = atoi(optarg);
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

            case 6: // --head
                ctx.head = atoi(optarg);
                break;

            case 7: // --tail
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

    // Set up callbacks
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

    // Output tail buffer if used
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

    // Print statistics if verbose
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

#endif // CISV_CLI
