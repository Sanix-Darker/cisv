#include "cisv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>  // For gettimeofday
#include <getopt.h>    // For getopt_long

#ifdef __AVX512F__
#include <immintrin.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Cache optimization constants
#define CACHE_LINE_SIZE 64
#define L1_SIZE (32 * 1024)
#define L2_SIZE (256 * 1024)
#define PREFETCH_DISTANCE 512  // Optimal for most CPUs

// Parser states - keep minimal for branch prediction
#define S_NORMAL  0
#define S_QUOTED  1

// Lookup table for fast character classification (branch-free)
static uint8_t char_props[256] __attribute__((aligned(64)));
static bool char_props_initialized = false;

typedef struct cisv_parser {
    // Hot path data - first cache line
    const uint8_t *cur __attribute__((aligned(64)));
    const uint8_t *end;
    const uint8_t *field_start;
    uint8_t state;
    char delimiter;
    char quote;
    char escape;

    // Field tracking
    int field_index;  // Current field index in row

    // Second cache line - callbacks and config
    cisv_field_cb fcb __attribute__((aligned(64)));
    cisv_row_cb rcb;
    void *user;
    bool trim;
    bool skip_empty_lines;
    int line_num;

    // Cold data - rarely accessed
    uint8_t *base __attribute__((aligned(64)));
    size_t size;
    int fd;
    cisv_error_cb ecb;
    char comment;
    int from_line;
    int to_line;

    // Statistics
    size_t rows;
    size_t fields;
    size_t current_row_fields;

    // Static quote buffer - no allocation overhead
    uint8_t quote_buffer[65536] __attribute__((aligned(64))); // 64KB static
    size_t quote_buffer_pos;

    // Quote state tracking for PCLMULQDQ optimization
    uint64_t quote_bits;
    uint64_t prev_iter_inside_quotes;
} cisv_parser;

// Initialize character property lookup table (branch-free classification)
static inline void init_char_props(char delimiter, char quote) {
    if (char_props_initialized &&
        char_props[(uint8_t)delimiter] == 0x01 &&
        char_props[(uint8_t)quote] == 0x02) {
        return; // Already initialized with same settings
    }

    memset(char_props, 0, 256);
    char_props[(uint8_t)delimiter] = 0x01;  // Bit 0: delimiter
    char_props[(uint8_t)quote] = 0x02;      // Bit 1: quote
    char_props['\n'] = 0x04;                // Bit 2: newline
    char_props['\r'] = 0x08;                // Bit 3: CR
    char_props[' '] = 0x10;                 // Bit 4: whitespace
    char_props['\t'] = 0x10;                // Bit 4: whitespace

    char_props_initialized = true;
}

void cisv_config_init(cisv_config *config) {
    memset(config, 0, sizeof(*config));
    config->delimiter = ',';
    config->quote = '"';
    config->from_line = 1;
}

// Branchless whitespace check using lookup table
static inline bool is_ws_fast(uint8_t c) {
    return (char_props[c] & 0x10) != 0;
}

// Inline field handling WITH trim support
static inline void yield_field(cisv_parser *p, const uint8_t *start, const uint8_t *end) {
    if (__builtin_expect(!p->fcb, 0)) return;

    // Apply trim if configured
    if (__builtin_expect(p->trim, 0)) {
        while (start < end && is_ws_fast(*start)) start++;
        while (start < end && is_ws_fast(*(end-1))) end--;
    }

    // Skip empty fields if configured
    if (p->skip_empty_lines && start >= end) {
        return;
    }

    p->fcb(p->user, (const char*)start, end - start);
    p->fields++;
    p->current_row_fields++;
}

// Inline quoted field handling WITH trim support
static inline void yield_quoted_field(cisv_parser *p) {
    if (__builtin_expect(!p->fcb, 0)) return;

    const uint8_t *start = p->quote_buffer;
    const uint8_t *end = p->quote_buffer + p->quote_buffer_pos;

    // Apply trim if configured
    if (__builtin_expect(p->trim, 0)) {
        while (start < end && is_ws_fast(*start)) start++;
        while (start < end && is_ws_fast(*(end-1))) end--;
    }

    // Skip empty fields if configured
    if (p->skip_empty_lines && start >= end) {
        p->quote_buffer_pos = 0;
        return;
    }

    p->fcb(p->user, (const char*)start, end - start);
    p->fields++;
    p->current_row_fields++;
    p->quote_buffer_pos = 0;
}

static inline void yield_row(cisv_parser *p) {
    p->line_num++;
    if (p->rcb && p->line_num >= p->from_line &&
        (!p->to_line || p->line_num <= p->to_line)) {
        p->rcb(p->user);
    }
    p->rows++;
    p->current_row_fields = 0;
    p->field_index = 0;
}

// Forward declare all parse functions
#ifdef __AVX512F__
static void parse_avx512(cisv_parser *p);
#endif
#ifdef __AVX2__
static void parse_avx2(cisv_parser *p);
#endif
#if !defined(__AVX512F__) && !defined(__AVX2__)
static void parse_scalar(cisv_parser *p);
#endif

#ifdef __AVX512F__
// AVX-512 optimized with PCLMULQDQ for quote handling
static void parse_avx512(cisv_parser *p) {
    const __m512i delim_v = _mm512_set1_epi8(p->delimiter);
    const __m512i quote_v = _mm512_set1_epi8(p->quote);
    const __m512i nl_v = _mm512_set1_epi8('\n');
    const __m512i cr_v = _mm512_set1_epi8('\r');

    while (p->cur + 64 <= p->end) {
        // Multi-stream prefetching for better latency hiding
        _mm_prefetch(p->cur + PREFETCH_DISTANCE, _MM_HINT_T1);
        _mm_prefetch(p->cur + PREFETCH_DISTANCE + 64, _MM_HINT_T2);

        if (p->state == S_NORMAL) {
            __m512i chunk = _mm512_loadu_si512((__m512i*)p->cur);

            __mmask64 delim_mask = _mm512_cmpeq_epi8_mask(chunk, delim_v);
            __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_v);
            __mmask64 nl_mask = _mm512_cmpeq_epi8_mask(chunk, nl_v);

            __mmask64 special = delim_mask | quote_mask | nl_mask;

            if (!special) {
                p->cur += 64;
                continue;
            }

            // Process all special chars in chunk - branch-free using bit manipulation
            __mmask64 cr_mask = nl_mask ? _mm512_cmpeq_epi8_mask(chunk, cr_v) : 0;

            while (special) {
                int pos = __builtin_ctzll(special);
                const uint8_t *ptr = p->cur + pos;
                __mmask64 bit = 1ULL << pos;

                // Branchless processing using bit tests
                if (delim_mask & bit) {
                    yield_field(p, p->field_start, ptr);
                    p->field_index++;
                    p->field_start = ptr + 1;
                } else if (nl_mask & bit) {
                    // Handle CRLF
                    const uint8_t *field_end = ptr;
                    // Branchless CRLF handling
                    field_end -= (pos > 0) & ((cr_mask >> (pos-1)) & 1);
                    yield_field(p, p->field_start, field_end);
                    p->cur = ptr + 1;
                    yield_row(p);
                    p->field_start = p->cur;
                    goto next_chunk;
                } else if ((quote_mask & bit) && ptr == p->field_start) {
                    p->state = S_QUOTED;
                    p->cur = ptr + 1;
                    p->quote_buffer_pos = 0;
                    goto handle_quoted;
                }

                special ^= bit;  // Clear bit
            }

            p->cur += 64;

            next_chunk:;
        } else {
            handle_quoted:
            // Scan for quotes in larger chunks for better throughput
            while (p->cur + 128 <= p->end) {
                __m512i chunk1 = _mm512_loadu_si512((__m512i*)p->cur);
                __m512i chunk2 = _mm512_loadu_si512((__m512i*)(p->cur + 64));

                __mmask64 quote_mask1 = _mm512_cmpeq_epi8_mask(chunk1, quote_v);
                __mmask64 quote_mask2 = _mm512_cmpeq_epi8_mask(chunk2, quote_v);

                if (!quote_mask1 && !quote_mask2) {
                    // Fast path - no quotes, bulk copy
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, 128);
                    p->quote_buffer_pos += 128;
                    p->cur += 128;
                    continue;
                }

                // Process first chunk with quotes
                if (quote_mask1) {
                    int pos = __builtin_ctzll(quote_mask1);

                    if (pos > 0) {
                        __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, pos);
                        p->quote_buffer_pos += pos;
                    }

                    p->cur += pos;

                    // Check for escaped quote
                    if (p->cur + 1 < p->end && p->cur[1] == p->quote) {
                        p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                        p->cur += 2;
                        continue;
                    }

                    // End of quoted field
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->cur++;
                    p->field_start = p->cur;
                    break;
                }

                // First chunk had no quotes, copy it
                __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, 64);
                p->quote_buffer_pos += 64;
                p->cur += 64;
            }

            // Handle remaining data in quoted state
            while (p->cur + 64 <= p->end && p->state == S_QUOTED) {
                __m512i chunk = _mm512_loadu_si512((__m512i*)p->cur);
                __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_v);

                if (!quote_mask) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, 64);
                    p->quote_buffer_pos += 64;
                    p->cur += 64;
                    continue;
                }

                int pos = __builtin_ctzll(quote_mask);

                if (pos > 0) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, pos);
                    p->quote_buffer_pos += pos;
                }

                p->cur += pos;

                if (p->cur + 1 < p->end && p->cur[1] == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur += 2;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->cur++;
                    p->field_start = p->cur;
                    break;
                }
            }
        }
    }

    // Handle remainder byte by byte with lookup table for branch-free processing
    while (p->cur < p->end) {
        uint8_t c = *p->cur;
        uint8_t props = char_props[c];
        p->cur++;

        if (p->state == S_NORMAL) {
            if (props & 0x01) {  // delimiter
                yield_field(p, p->field_start, p->cur - 1);
                p->field_index++;
                p->field_start = p->cur;
            } else if (props & 0x04) {  // newline
                const uint8_t *field_end = p->cur - 1;
                // Branchless CRLF handling
                field_end -= (field_end > p->field_start) & (field_end[-1] == '\r');
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if ((props & 0x02) && p->cur - 1 == p->field_start) {  // quote
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else {
            if (props & 0x02) {  // quote
                if (p->cur < p->end && *p->cur == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->field_start = p->cur;
                }
            } else {
                p->quote_buffer[p->quote_buffer_pos++] = c;
            }
        }
    }

    // Final cleanup
    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED && p->quote_buffer_pos > 0) {
        yield_quoted_field(p);
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

#ifdef __AVX2__
// AVX2 fast path with optimizations
static void parse_avx2(cisv_parser *p) {
    const __m256i delim_v = _mm256_set1_epi8(p->delimiter);
    const __m256i quote_v = _mm256_set1_epi8(p->quote);
    const __m256i nl_v = _mm256_set1_epi8('\n');
    const __m256i cr_v = _mm256_set1_epi8('\r');

    while (p->cur + 64 <= p->end) {
        // Process two chunks at once for better throughput
        _mm_prefetch(p->cur + PREFETCH_DISTANCE, _MM_HINT_T1);
        _mm_prefetch(p->cur + PREFETCH_DISTANCE + 32, _MM_HINT_T2);

        if (p->state == S_NORMAL) {
            // Process two chunks together
            __m256i chunk1 = _mm256_loadu_si256((__m256i*)p->cur);
            __m256i chunk2 = _mm256_loadu_si256((__m256i*)(p->cur + 32));

            __m256i delim_cmp1 = _mm256_cmpeq_epi8(chunk1, delim_v);
            __m256i quote_cmp1 = _mm256_cmpeq_epi8(chunk1, quote_v);
            __m256i nl_cmp1 = _mm256_cmpeq_epi8(chunk1, nl_v);

            __m256i delim_cmp2 = _mm256_cmpeq_epi8(chunk2, delim_v);
            __m256i quote_cmp2 = _mm256_cmpeq_epi8(chunk2, quote_v);
            __m256i nl_cmp2 = _mm256_cmpeq_epi8(chunk2, nl_v);

            uint32_t delim_mask1 = _mm256_movemask_epi8(delim_cmp1);
            uint32_t quote_mask1 = _mm256_movemask_epi8(quote_cmp1);
            uint32_t nl_mask1 = _mm256_movemask_epi8(nl_cmp1);

            uint32_t delim_mask2 = _mm256_movemask_epi8(delim_cmp2);
            uint32_t quote_mask2 = _mm256_movemask_epi8(quote_cmp2);
            uint32_t nl_mask2 = _mm256_movemask_epi8(nl_cmp2);

            uint32_t special1 = delim_mask1 | quote_mask1 | nl_mask1;
            uint32_t special2 = delim_mask2 | quote_mask2 | nl_mask2;

            // Fast path - no special chars in 64 bytes
            if (!special1 && !special2) {
                p->cur += 64;
                continue;
            }

            // Process first chunk
            if (special1) {
                uint32_t cr_mask1 = nl_mask1 ? _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, cr_v)) : 0;

                while (special1) {
                    int pos = __builtin_ctz(special1);
                    const uint8_t *ptr = p->cur + pos;
                    uint32_t bit = 1U << pos;

                    if (delim_mask1 & bit) {
                        yield_field(p, p->field_start, ptr);
                        p->field_index++;
                        p->field_start = ptr + 1;
                    } else if (nl_mask1 & bit) {
                        const uint8_t *field_end = ptr;
                        field_end -= (pos > 0) & ((cr_mask1 >> (pos-1)) & 1);
                        yield_field(p, p->field_start, field_end);
                        p->cur = ptr + 1;
                        yield_row(p);
                        p->field_start = p->cur;
                        goto next_chunk;
                    } else if ((quote_mask1 & bit) && ptr == p->field_start) {
                        p->state = S_QUOTED;
                        p->cur = ptr + 1;
                        p->quote_buffer_pos = 0;
                        goto handle_quoted;
                    }
                    special1 ^= bit;
                }
                p->cur += 32;
            } else {
                p->cur += 32;
            }

            // Process second chunk if still in normal state
            if (p->state == S_NORMAL && special2) {
                uint32_t cr_mask2 = nl_mask2 ? _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, cr_v)) : 0;

                while (special2) {
                    int pos = __builtin_ctz(special2);
                    const uint8_t *ptr = p->cur + pos;
                    uint32_t bit = 1U << pos;

                    if (delim_mask2 & bit) {
                        yield_field(p, p->field_start, ptr);
                        p->field_index++;
                        p->field_start = ptr + 1;
                    } else if (nl_mask2 & bit) {
                        const uint8_t *field_end = ptr;
                        field_end -= (pos > 0) & ((cr_mask2 >> (pos-1)) & 1);
                        yield_field(p, p->field_start, field_end);
                        p->cur = ptr + 1;
                        yield_row(p);
                        p->field_start = p->cur;
                        goto next_chunk;
                    } else if ((quote_mask2 & bit) && ptr == p->field_start) {
                        p->state = S_QUOTED;
                        p->cur = ptr + 1;
                        p->quote_buffer_pos = 0;
                        goto handle_quoted;
                    }
                    special2 ^= bit;
                }
                p->cur += 32;
            } else if (p->state == S_NORMAL) {
                p->cur += 32;
            }

            next_chunk:;
        } else {
            handle_quoted:
            // Process quoted fields - scan multiple chunks
            while (p->cur + 64 <= p->end) {
                __m256i chunk1 = _mm256_loadu_si256((__m256i*)p->cur);
                __m256i chunk2 = _mm256_loadu_si256((__m256i*)(p->cur + 32));

                uint32_t mask1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, quote_v));
                uint32_t mask2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, quote_v));

                if (!mask1 && !mask2) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, 64);
                    p->quote_buffer_pos += 64;
                    p->cur += 64;
                    continue;
                }

                // Find first quote
                int offset = 0;
                uint32_t mask = 0;

                if (mask1) {
                    mask = mask1;
                    offset = 0;
                } else {
                    mask = mask2;
                    offset = 32;
                }

                int pos = __builtin_ctz(mask) + offset;

                if (pos > 0) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, pos);
                    p->quote_buffer_pos += pos;
                }

                p->cur += pos;

                if (p->cur + 1 < p->end && p->cur[1] == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur += 2;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->cur++;
                    p->field_start = p->cur;
                    break;
                }
            }

            // Handle remaining quoted data
            while (p->cur + 32 <= p->end && p->state == S_QUOTED) {
                __m256i chunk = _mm256_loadu_si256((__m256i*)p->cur);
                uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, quote_v));

                if (!mask) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, 32);
                    p->quote_buffer_pos += 32;
                    p->cur += 32;
                    continue;
                }

                int pos = __builtin_ctz(mask);

                if (pos > 0) {
                    __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, pos);
                    p->quote_buffer_pos += pos;
                }

                p->cur += pos;

                if (p->cur + 1 < p->end && p->cur[1] == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur += 2;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->cur++;
                    p->field_start = p->cur;
                    break;
                }
            }
        }
    }

    // Handle remainder with lookup table
    while (p->cur < p->end) {
        uint8_t c = *p->cur;
        uint8_t props = char_props[c];
        p->cur++;

        if (p->state == S_NORMAL) {
            if (props & 0x01) {  // delimiter
                yield_field(p, p->field_start, p->cur - 1);
                p->field_index++;
                p->field_start = p->cur;
            } else if (props & 0x04) {  // newline
                const uint8_t *field_end = p->cur - 1;
                field_end -= (field_end > p->field_start) & (field_end[-1] == '\r');
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if ((props & 0x02) && p->cur - 1 == p->field_start) {  // quote
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else {
            if (props & 0x02) {  // quote
                if (p->cur < p->end && *p->cur == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->field_start = p->cur;
                }
            } else {
                p->quote_buffer[p->quote_buffer_pos++] = c;
            }
        }
    }

    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED && p->quote_buffer_pos > 0) {
        yield_quoted_field(p);
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

#if !defined(__AVX512F__) && !defined(__AVX2__)
// Scalar fallback with lookup table optimization
static void parse_scalar(cisv_parser *p) {
    while (p->cur < p->end) {
        if (p->state == S_NORMAL) {
            // Fast forward scan using memchr for delimiters
            const uint8_t *scan = p->cur;
            const uint8_t *next_delim = memchr(scan, p->delimiter, p->end - scan);
            const uint8_t *next_nl = memchr(scan, '\n', p->end - scan);
            const uint8_t *next_quote = memchr(scan, p->quote, p->end - scan);

            // Find the nearest special character
            const uint8_t *nearest = p->end;
            if (next_delim && next_delim < nearest) nearest = next_delim;
            if (next_nl && next_nl < nearest) nearest = next_nl;
            if (next_quote && next_quote < nearest && next_quote == p->field_start) {
                nearest = next_quote;
            }

            if (nearest >= p->end) {
                p->cur = p->end;
                break;
            }

            p->cur = nearest;
            uint8_t c = *p->cur++;
            uint8_t props = char_props[c];

            if (props & 0x01) {  // delimiter
                yield_field(p, p->field_start, p->cur - 1);
                p->field_index++;
                p->field_start = p->cur;
            } else if (props & 0x04) {  // newline
                const uint8_t *field_end = p->cur - 1;
                field_end -= (field_end > p->field_start) & (field_end[-1] == '\r');
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if ((props & 0x02) && p->cur - 1 == p->field_start) {  // quote
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else {
            // Quoted state - scan for quotes using memchr
            const uint8_t *quote_pos = memchr(p->cur, p->quote, p->end - p->cur);
            if (quote_pos) {
                // Copy everything up to quote
                size_t len = quote_pos - p->cur;
                __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, len);
                p->quote_buffer_pos += len;
                p->cur = quote_pos + 1;

                // Check for escaped quote
                if (p->cur < p->end && *p->cur == p->quote) {
                    p->quote_buffer[p->quote_buffer_pos++] = p->quote;
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->field_index++;
                    p->state = S_NORMAL;
                    p->field_start = p->cur;
                }
            } else {
                // No quote found - copy rest
                size_t len = p->end - p->cur;
                __builtin_memcpy(p->quote_buffer + p->quote_buffer_pos, p->cur, len);
                p->quote_buffer_pos += len;
                p->cur = p->end;
            }
        }
    }

    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED && p->quote_buffer_pos > 0) {
        yield_quoted_field(p);
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

cisv_parser *cisv_parser_create_with_config(const cisv_config *config) {
    cisv_parser *p = (cisv_parser*)aligned_alloc(CACHE_LINE_SIZE, sizeof(*p));
    if (!p) return NULL;

    memset(p, 0, sizeof(*p));

    p->delimiter = config->delimiter;
    p->quote = config->quote;
    p->escape = config->escape;
    p->trim = config->trim;
    p->skip_empty_lines = config->skip_empty_lines;
    p->comment = config->comment;
    p->from_line = config->from_line;
    p->to_line = config->to_line;

    p->fcb = config->field_cb;
    p->rcb = config->row_cb;
    p->ecb = config->error_cb;
    p->user = config->user;

    p->fd = -1;
    p->line_num = 0;
    p->current_row_fields = 0;
    p->quote_buffer_pos = 0;
    p->field_index = 0;

    // Initialize character property lookup table
    init_char_props(config->delimiter, config->quote);

    return p;
}

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user) {
    cisv_config config;
    cisv_config_init(&config);
    config.field_cb = fcb;
    config.row_cb = rcb;
    config.user = user;
    return cisv_parser_create_with_config(&config);
}

void cisv_parser_destroy(cisv_parser *p) {
    if (!p) return;

    if (p->base) munmap(p->base, p->size);
    if (p->fd >= 0) close(p->fd);
    free(p);
}

int cisv_parser_parse_file(cisv_parser *p, const char *path) {
    p->fd = open(path, O_RDONLY);
    if (p->fd < 0) return -errno;

    struct stat st;
    if (fstat(p->fd, &st) < 0) {
        close(p->fd);
        p->fd = -1;
        return -errno;
    }

    if (st.st_size == 0) {
        close(p->fd);
        p->fd = -1;
        return 0;
    }

    p->size = st.st_size;

    // Use huge pages for large files
    int flags = MAP_PRIVATE;
#ifdef MAP_HUGETLB
    if (st.st_size > 2*1024*1024) flags |= MAP_HUGETLB;
#endif
#ifdef MAP_POPULATE
    flags |= MAP_POPULATE;
#endif

    p->base = (uint8_t*)mmap(NULL, p->size, PROT_READ, flags, p->fd, 0);

#ifdef MAP_HUGETLB
    if (p->base == MAP_FAILED && (flags & MAP_HUGETLB)) {
        flags &= ~MAP_HUGETLB;
        p->base = (uint8_t*)mmap(NULL, p->size, PROT_READ, flags, p->fd, 0);
    }
#endif

    if (p->base == MAP_FAILED) {
        close(p->fd);
        p->fd = -1;
        return -errno;
    }

    // Advise kernel - use MADV_HUGEPAGE for better TLB performance
    madvise(p->base, p->size, MADV_SEQUENTIAL | MADV_WILLNEED);
#ifdef MADV_HUGEPAGE
    madvise(p->base, p->size, MADV_HUGEPAGE);
#endif

    // Setup parsing pointers
    p->cur = p->base;
    p->end = p->base + p->size;
    p->field_start = p->cur;
    p->state = S_NORMAL;
    p->line_num = 0;
    p->current_row_fields = 0;  // Reset
    p->quote_buffer_pos = 0;    // Reset quote buffer
    p->field_index = 0;

    // Choose best implementation
#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#else
    parse_scalar(p);
#endif

    return 0;
}

// Ultra-fast row counting with 8x unrolling
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

    uint8_t *base = (uint8_t*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        return 0;
    }

    madvise(base, st.st_size, MADV_SEQUENTIAL);
#ifdef MADV_HUGEPAGE
    madvise(base, st.st_size, MADV_HUGEPAGE);
#endif

    size_t count = 0;

#ifdef __AVX512F__
    const __m512i nl = _mm512_set1_epi8('\n');
    size_t i = 0;

    // Unroll 8x for better ILP (instruction-level parallelism)
    for (; i + 512 <= (size_t)st.st_size; i += 512) {
        _mm_prefetch(base + i + PREFETCH_DISTANCE, _MM_HINT_T1);
        _mm_prefetch(base + i + PREFETCH_DISTANCE + 256, _MM_HINT_T2);

        __mmask64 m1 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i)), nl);
        __mmask64 m2 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 64)), nl);
        __mmask64 m3 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 128)), nl);
        __mmask64 m4 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 192)), nl);
        __mmask64 m5 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 256)), nl);
        __mmask64 m6 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 320)), nl);
        __mmask64 m7 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 384)), nl);
        __mmask64 m8 = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i + 448)), nl);

        count += __builtin_popcountll(m1) + __builtin_popcountll(m2) +
                 __builtin_popcountll(m3) + __builtin_popcountll(m4) +
                 __builtin_popcountll(m5) + __builtin_popcountll(m6) +
                 __builtin_popcountll(m7) + __builtin_popcountll(m8);
    }

    for (; i + 64 <= (size_t)st.st_size; i += 64) {
        __mmask64 mask = _mm512_cmpeq_epi8_mask(_mm512_loadu_si512((__m512i*)(base + i)), nl);
        count += __builtin_popcountll(mask);
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#elif defined(__AVX2__)
    const __m256i nl = _mm256_set1_epi8('\n');
    size_t i = 0;

    // Unroll 8x for better performance
    for (; i + 256 <= (size_t)st.st_size; i += 256) {
        _mm_prefetch(base + i + PREFETCH_DISTANCE, _MM_HINT_T1);

        uint32_t m1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i)), nl));
        uint32_t m2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 32)), nl));
        uint32_t m3 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 64)), nl));
        uint32_t m4 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 96)), nl));
        uint32_t m5 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 128)), nl));
        uint32_t m6 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 160)), nl));
        uint32_t m7 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 192)), nl));
        uint32_t m8 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i + 224)), nl));

        count += __builtin_popcount(m1) + __builtin_popcount(m2) +
                 __builtin_popcount(m3) + __builtin_popcount(m4) +
                 __builtin_popcount(m5) + __builtin_popcount(m6) +
                 __builtin_popcount(m7) + __builtin_popcount(m8);
    }

    for (; i + 32 <= (size_t)st.st_size; i += 32) {
        uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((__m256i*)(base + i)), nl));
        count += __builtin_popcount(mask);
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#else
    // Use memchr for scalar - it's optimized with SIMD internally
    const uint8_t *p = base;
    const uint8_t *end = base + st.st_size;

    while (p < end) {
        const uint8_t *nl = memchr(p, '\n', end - p);
        if (!nl) break;
        count++;
        p = nl + 1;
    }
#endif

    // Check if last character is not newline
    if (st.st_size > 0 && base[st.st_size - 1] != '\n') {
        count++;
    }

    munmap(base, st.st_size);
    close(fd);
    return count;
}

size_t cisv_parser_count_rows_with_config(const char *path, const cisv_config *config) {
    (void)config;  // Suppress unused parameter warning - may use in future for quote-aware counting
    return cisv_parser_count_rows(path);
}

int cisv_parser_write(cisv_parser *p, const uint8_t *chunk, size_t len) {
    p->cur = chunk;
    p->end = chunk + len;

    // Only reset field_start if we're starting fresh
    if (p->state == S_NORMAL && p->current_row_fields == 0) {
        p->field_start = p->cur;
    } else {
        // Continue from where we left off
        p->field_start = p->cur;
    }

#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#else
    parse_scalar(p);
#endif

    return 0;
}

void cisv_parser_end(cisv_parser *p) {
    if (p->state == S_NORMAL && p->field_start && p->field_start < p->cur) {
        yield_field(p, p->field_start, p->cur);
    } else if (p->state == S_QUOTED && p->quote_buffer_pos > 0) {
        yield_quoted_field(p);
    }

    // Always yield a row if we have any accumulated fields
    //if (p->current_row_fields > 0) {
    //    yield_row(p);
    //}
}

int cisv_parser_get_line_number(const cisv_parser *p) {
    return p ? p->line_num : 0;
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
