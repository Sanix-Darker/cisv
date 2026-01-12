#include "cisv/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>  // For INT_MAX
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

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
#define PREFETCH_DISTANCE 1024

// Parser states - keep minimal for branch prediction
#define S_NORMAL  0
#define S_QUOTED  1
#define S_ESCAPE  2

typedef struct cisv_parser {
    // Hot path data - first cache line
    const uint8_t *cur __attribute__((aligned(64)));
    const uint8_t *end;
    const uint8_t *field_start;
    uint8_t state;
    char delimiter;
    char quote;
    char escape;

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

    // Buffer for accumulating quoted field content
    uint8_t *quote_buffer __attribute__((aligned(64)));
    size_t quote_buffer_size;
    size_t quote_buffer_pos;

    // Buffer for streaming mode - holds partial unquoted fields across chunks
    uint8_t *stream_buffer;
    size_t stream_buffer_size;
    size_t stream_buffer_pos;
    bool streaming_mode;
} cisv_parser;

// Ultra-fast whitespace lookup table - O(1) direct index instead of bit extraction
// Covers: space (32), tab (9), CR (13), LF (10)
static const uint8_t ws_lookup[256] = {
    [' '] = 1, ['\t'] = 1, ['\r'] = 1, ['\n'] = 1
};

#define is_ws(c) (ws_lookup[(uint8_t)(c)])

// =============================================================================
// SWAR (SIMD Within A Register) - 1 Billion Row Challenge technique
// Processes 8 bytes at a time without SIMD instructions
// =============================================================================

// Check if any byte in a 64-bit word equals a target character
// Returns non-zero mask with high bit set for each matching byte
static inline uint64_t swar_has_byte(uint64_t word, uint8_t target) {
    uint64_t mask = target * 0x0101010101010101ULL;
    uint64_t xored = word ^ mask;
    // High bit set if any byte is zero (i.e., was a match)
    return (xored - 0x0101010101010101ULL) & ~xored & 0x8080808080808080ULL;
}

// Find position of first matching byte (0-7), or 8 if none
static inline int swar_find_first(uint64_t match_mask) {
    if (!match_mask) return 8;
    return __builtin_ctzll(match_mask) >> 3;
}

// Check if word contains delimiter, newline, or quote
static inline uint64_t swar_has_special(uint64_t word, char delim, char quote) {
    return swar_has_byte(word, delim) |
           swar_has_byte(word, '\n') |
           swar_has_byte(word, quote);
}

// SIMD-accelerated whitespace trimming
#ifdef __AVX2__
// Skip leading whitespace using AVX2 - processes 32 bytes at a time
static inline const uint8_t *skip_ws_avx2(const uint8_t *start, const uint8_t *end) {
    const __m256i space = _mm256_set1_epi8(' ');
    const __m256i tab = _mm256_set1_epi8('\t');
    const __m256i cr = _mm256_set1_epi8('\r');
    const __m256i nl = _mm256_set1_epi8('\n');

    while (start + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)start);

        // Check for any of the 4 whitespace characters
        __m256i is_space = _mm256_cmpeq_epi8(chunk, space);
        __m256i is_tab = _mm256_cmpeq_epi8(chunk, tab);
        __m256i is_cr = _mm256_cmpeq_epi8(chunk, cr);
        __m256i is_nl = _mm256_cmpeq_epi8(chunk, nl);

        __m256i is_ws_vec = _mm256_or_si256(
            _mm256_or_si256(is_space, is_tab),
            _mm256_or_si256(is_cr, is_nl)
        );

        // Get mask of non-whitespace bytes
        uint32_t mask = ~_mm256_movemask_epi8(is_ws_vec);

        if (mask) {
            // Found non-whitespace byte, return position of first one
            return start + __builtin_ctz(mask);
        }
        start += 32;
    }

    // Scalar fallback for remainder
    while (start < end && is_ws(*start)) start++;
    return start;
}

// Find last non-whitespace using AVX2 - scans backwards
static inline const uint8_t *rskip_ws_avx2(const uint8_t *start, const uint8_t *end) {
    const __m256i space = _mm256_set1_epi8(' ');
    const __m256i tab = _mm256_set1_epi8('\t');
    const __m256i cr = _mm256_set1_epi8('\r');
    const __m256i nl = _mm256_set1_epi8('\n');

    while (end - 32 >= start) {
        const uint8_t *check = end - 32;
        __m256i chunk = _mm256_loadu_si256((const __m256i*)check);

        __m256i is_space = _mm256_cmpeq_epi8(chunk, space);
        __m256i is_tab = _mm256_cmpeq_epi8(chunk, tab);
        __m256i is_cr = _mm256_cmpeq_epi8(chunk, cr);
        __m256i is_nl = _mm256_cmpeq_epi8(chunk, nl);

        __m256i is_ws_vec = _mm256_or_si256(
            _mm256_or_si256(is_space, is_tab),
            _mm256_or_si256(is_cr, is_nl)
        );

        uint32_t mask = ~_mm256_movemask_epi8(is_ws_vec);

        if (mask) {
            // Found non-whitespace, return position after last one
            return check + 32 - __builtin_clz(mask);
        }
        end -= 32;
    }

    // Scalar fallback
    while (start < end && is_ws(*(end - 1))) end--;
    return end;
}
#endif

void cisv_config_init(cisv_config *config) {
    memset(config, 0, sizeof(*config));
    config->delimiter = ',';
    config->quote = '"';
    config->from_line = 1;
}

// Maximum quote buffer size to prevent DoS (100MB)
#define MAX_QUOTE_BUFFER_SIZE (100 * 1024 * 1024)
// Minimum buffer increment for efficiency (64KB)
#define MIN_BUFFER_INCREMENT (64 * 1024)
// Default maximum field size (1MB) - configurable
#define DEFAULT_MAX_FIELD_SIZE (1 * 1024 * 1024)

// Ensure quote buffer has enough space
// Optimized: power-of-2 growth with 64KB minimum increment, cache-line aligned
static inline bool ensure_quote_buffer(cisv_parser *p, size_t needed) {
    size_t required = p->quote_buffer_pos + needed;
    if (__builtin_expect(required <= p->quote_buffer_size, 1)) {
        return true;  // Fast path: buffer has space
    }

    // Check for overflow using compiler builtin
    size_t new_size;
    if (__builtin_add_overflow(p->quote_buffer_pos, needed, &new_size)) {
        return false;
    }

    // Power-of-2 growth with 64KB minimum increment
    if (new_size < MIN_BUFFER_INCREMENT) {
        new_size = MIN_BUFFER_INCREMENT;
    } else {
        // Round up to next power of 2
        new_size--;
        new_size |= new_size >> 1;
        new_size |= new_size >> 2;
        new_size |= new_size >> 4;
        new_size |= new_size >> 8;
        new_size |= new_size >> 16;
        new_size |= new_size >> 32;
        new_size++;
    }

    // Align to cache line for SIMD access
    new_size = (new_size + CACHE_LINE_SIZE - 1) & ~(size_t)(CACHE_LINE_SIZE - 1);

    // Enforce maximum buffer size to prevent DoS
    if (new_size > MAX_QUOTE_BUFFER_SIZE) return false;

    void *tmp = realloc(p->quote_buffer, new_size);
    if (__builtin_expect(!tmp, 0)) return false;
    p->quote_buffer = tmp;
    p->quote_buffer_size = new_size;
    return true;
}

// Append to quote buffer
static inline bool append_to_quote_buffer(cisv_parser *p, const uint8_t *data, size_t len) {
    if (!ensure_quote_buffer(p, len)) return false;
    memcpy(p->quote_buffer + p->quote_buffer_pos, data, len);
    p->quote_buffer_pos += len;
    return true;
}

// Ensure stream buffer has enough space (for streaming mode partial fields)
// SECURITY: Uses compiler built-ins for overflow-safe arithmetic
static inline bool ensure_stream_buffer(cisv_parser *p, size_t needed) {
    // Check required size with overflow protection
    size_t required;
    if (__builtin_add_overflow(p->stream_buffer_pos, needed, &required)) {
        if (p->ecb) p->ecb(p->user, p->line_num, "Stream buffer size overflow");
        return false;
    }

    if (required <= p->stream_buffer_size) {
        return true;  // Fast path: buffer has space
    }

    // Calculate new size with overflow protection
    size_t sum;
    if (__builtin_add_overflow(p->stream_buffer_size, needed, &sum)) {
        if (p->ecb) p->ecb(p->user, p->line_num, "Stream buffer calculation overflow");
        return false;
    }

    size_t new_size;
    if (__builtin_mul_overflow(sum, (size_t)2, &new_size)) {
        if (p->ecb) p->ecb(p->user, p->line_num, "Stream buffer growth overflow");
        return false;
    }

    // Enforce maximum buffer size to prevent DoS
    if (new_size > MAX_QUOTE_BUFFER_SIZE) {
        if (p->ecb) p->ecb(p->user, p->line_num, "Stream buffer exceeds maximum size");
        return false;
    }

    void *tmp = realloc(p->stream_buffer, new_size);
    if (!tmp) {
        // SECURITY: On realloc failure, old buffer is still valid
        // Report error but don't invalidate existing buffer
        if (p->ecb) p->ecb(p->user, p->line_num, "Stream buffer allocation failed");
        return false;
    }
    p->stream_buffer = tmp;
    p->stream_buffer_size = new_size;
    return true;
}

// Append to stream buffer
static inline bool append_to_stream_buffer(cisv_parser *p, const uint8_t *data, size_t len) {
    if (!ensure_stream_buffer(p, len)) return false;
    memcpy(p->stream_buffer + p->stream_buffer_pos, data, len);
    p->stream_buffer_pos += len;
    return true;
}

// Inline hot-path functions
static inline void yield_field(cisv_parser *p, const uint8_t *start, const uint8_t *end) {
    if (!p->fcb) return;

    // In streaming mode, check if we have buffered partial field data
    // SECURITY: Add NULL check for stream_buffer to prevent NULL dereference
    if (p->streaming_mode && p->stream_buffer_pos > 0 && p->stream_buffer) {
        // Append current field data to stream buffer and yield from there
        size_t current_len = end - start;
        if (current_len > 0) {
            if (!append_to_stream_buffer(p, start, current_len)) {
                // Buffer overflow - report error and yield what we have from original pointers
                if (p->ecb) p->ecb(p->user, p->line_num, "Field exceeds maximum buffer size");
                p->stream_buffer_pos = 0;
                // Don't return - yield the original field data
            } else {
                start = p->stream_buffer;
                end = p->stream_buffer + p->stream_buffer_pos;
            }
        } else {
            start = p->stream_buffer;
            end = p->stream_buffer + p->stream_buffer_pos;
        }
    }

    if (__builtin_expect(p->trim, 0)) {
#ifdef __AVX2__
        // Use SIMD for fields larger than 64 bytes, scalar for smaller
        size_t len = end - start;
        if (__builtin_expect(len >= 64, 0)) {
            start = skip_ws_avx2(start, end);
            if (start < end) {
                end = rskip_ws_avx2(start, end);
            }
        } else {
            // Scalar path for small fields
            while (start < end && is_ws(*start)) start++;
            while (start < end && is_ws(*(end-1))) end--;
        }
#else
        // Trim leading whitespace - expect few iterations
        while (start < end && __builtin_expect(is_ws(*start), 0)) start++;
        // Trim trailing whitespace - expect few iterations
        while (start < end && __builtin_expect(is_ws(*(end-1)), 0)) end--;
#endif
    }

    if (__builtin_expect(start < end || !p->skip_empty_lines, 1)) {
        p->fcb(p->user, (const char*)start, end - start);
        p->fields++;
        p->current_row_fields++;
    }

    // Clear stream buffer after yielding
    if (__builtin_expect(p->streaming_mode, 0)) {
        p->stream_buffer_pos = 0;
    }
}

// Yield field from quote buffer
static inline void yield_quoted_field(cisv_parser *p) {
    if (!p->fcb) return;

    const uint8_t *start = p->quote_buffer;
    const uint8_t *end = p->quote_buffer + p->quote_buffer_pos;

    if (__builtin_expect(p->trim, 0)) {
        // Trim leading whitespace - expect few iterations
        while (start < end && __builtin_expect(is_ws(*start), 0)) start++;
        // Trim trailing whitespace - expect few iterations
        while (start < end && __builtin_expect(is_ws(*(end-1)), 0)) end--;
    }

    if (__builtin_expect(start < end || !p->skip_empty_lines, 1)) {
        p->fcb(p->user, (const char*)start, end - start);
        p->fields++;
        p->current_row_fields++;
    }

    p->quote_buffer_pos = 0;
}

static inline void yield_row(cisv_parser *p) {
    // SECURITY: Protect against line number overflow (2+ billion rows)
    if (__builtin_expect(p->line_num < INT_MAX, 1)) {
        p->line_num++;
    }
    // Skip empty rows if skip_empty_lines is enabled
    if (p->skip_empty_lines && p->current_row_fields == 0) {
        return;
    }
    if (p->rcb && p->line_num >= p->from_line &&
        (!p->to_line || p->line_num <= p->to_line)) {
        p->rcb(p->user);
    }
    p->rows++;
    p->current_row_fields = 0;
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
// AVX-512 ultra-fast path
static void parse_avx512(cisv_parser *p) {
    const __m512i delim_v = _mm512_set1_epi8(p->delimiter);
    const __m512i quote_v = _mm512_set1_epi8(p->quote);
    const __m512i nl_v = _mm512_set1_epi8('\n');
    const __m512i cr_v = _mm512_set1_epi8('\r');

    while (p->cur + 64 <= p->end) {
        _mm_prefetch(p->cur + PREFETCH_DISTANCE, _MM_HINT_T0);
        _mm_prefetch(p->cur + PREFETCH_DISTANCE + 64, _MM_HINT_T0);

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

            while (special) {
                int pos = __builtin_ctzll(special);
                const uint8_t *ptr = p->cur + pos;

                if (delim_mask & (1ULL << pos)) {
                    yield_field(p, p->field_start, ptr);
                    p->field_start = ptr + 1;
                } else if (nl_mask & (1ULL << pos)) {
                    const uint8_t *field_end = ptr;
                    if (field_end > p->field_start && *(field_end - 1) == '\r') {
                        field_end--;
                    }
                    yield_field(p, p->field_start, field_end);
                    yield_row(p);
                    p->field_start = ptr + 1;
                } else if (quote_mask & (1ULL << pos)) {
                    p->state = S_QUOTED;
                    p->cur = ptr + 1;
                    p->quote_buffer_pos = 0;
                    goto handle_quoted;
                }

                special &= special - 1;
            }

            p->cur += 64;
        } else {
            handle_quoted:
            while (p->cur + 64 <= p->end) {
                __m512i chunk = _mm512_loadu_si512((__m512i*)p->cur);
                __mmask64 quote_mask = _mm512_cmpeq_epi8_mask(chunk, quote_v);

                if (!quote_mask) {
                    append_to_quote_buffer(p, p->cur, 64);
                    p->cur += 64;
                    continue;
                }

                int pos = __builtin_ctzll(quote_mask);

                if (pos > 0) {
                    append_to_quote_buffer(p, p->cur, pos);
                }

                p->cur += pos;

                if (p->cur + 1 < p->end && *(p->cur + 1) == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur += 2;
                    continue;
                }

                yield_quoted_field(p);
                p->state = S_NORMAL;
                p->cur++;
                // Skip delimiter/newline immediately after closing quote
                if (p->cur < p->end && *p->cur == p->delimiter) {
                    p->cur++;
                } else if (p->cur < p->end && *p->cur == '\n') {
                    p->cur++;
                    yield_row(p);
                } else if (p->cur < p->end && *p->cur == '\r' &&
                           p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                    p->cur += 2;
                    yield_row(p);
                }
                p->field_start = p->cur;
                break;
            }
        }
    }

    // Handle remainder
    while (p->cur < p->end) {
        uint8_t c = *p->cur++;

        if (p->state == S_NORMAL) {
            if (c == p->delimiter) {
                yield_field(p, p->field_start, p->cur - 1);
                p->field_start = p->cur;
            } else if (c == '\n') {
                const uint8_t *field_end = p->cur - 1;
                if (field_end > p->field_start && *(field_end - 1) == '\r') {
                    field_end--;
                }
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if (c == p->quote && p->cur - 1 == p->field_start) {
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else if (p->state == S_QUOTED) {
            if (c == p->quote) {
                if (p->cur < p->end && *p->cur == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->state = S_NORMAL;
                    // Skip delimiter/newline immediately after closing quote
                    if (p->cur < p->end && *p->cur == p->delimiter) {
                        p->cur++;
                    } else if (p->cur < p->end && *p->cur == '\n') {
                        p->cur++;
                        yield_row(p);
                    } else if (p->cur < p->end && *p->cur == '\r' &&
                               p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                        p->cur += 2;
                        yield_row(p);
                    }
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
            }
        }
    }

    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED) {
        // SECURITY: Report unterminated quote at EOF
        if (p->ecb) {
            p->ecb(p->user, p->line_num, "Unterminated quoted field at EOF");
        }
        // Still yield the partial content so data isn't lost
        if (p->quote_buffer_pos > 0) {
            yield_quoted_field(p);
        }
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

#ifdef __AVX2__
// AVX2 fast path
static void parse_avx2(cisv_parser *p) {
    const __m256i delim_v = _mm256_set1_epi8(p->delimiter);
    const __m256i quote_v = _mm256_set1_epi8(p->quote);
    const __m256i nl_v = _mm256_set1_epi8('\n');

    while (p->cur + 32 <= p->end) {
        _mm_prefetch(p->cur + PREFETCH_DISTANCE, _MM_HINT_T0);

        if (p->state == S_NORMAL) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)p->cur);

            __m256i delim_cmp = _mm256_cmpeq_epi8(chunk, delim_v);
            __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, quote_v);
            __m256i nl_cmp = _mm256_cmpeq_epi8(chunk, nl_v);

            __m256i special = _mm256_or_si256(delim_cmp, _mm256_or_si256(quote_cmp, nl_cmp));
            uint32_t mask = _mm256_movemask_epi8(special);

            if (!mask) {
                p->cur += 32;
                continue;
            }

            while (mask) {
                int pos = __builtin_ctz(mask);
                const uint8_t *ptr = p->cur + pos;
                uint8_t c = *ptr;

                if (c == p->delimiter) {
                    yield_field(p, p->field_start, ptr);
                    p->field_start = ptr + 1;
                } else if (c == '\n') {
                    const uint8_t *field_end = ptr;
                    if (field_end > p->field_start && *(field_end - 1) == '\r') {
                        field_end--;
                    }
                    yield_field(p, p->field_start, field_end);
                    yield_row(p);
                    p->field_start = ptr + 1;
                } else if (c == p->quote) {
                    p->state = S_QUOTED;
                    p->cur = ptr + 1;
                    p->quote_buffer_pos = 0;
                    goto handle_quoted;
                }

                mask &= mask - 1;
            }

            p->cur += 32;
        } else {
            handle_quoted:
            while (p->cur + 32 <= p->end) {
                __m256i chunk = _mm256_loadu_si256((__m256i*)p->cur);
                __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, quote_v);
                uint32_t mask = _mm256_movemask_epi8(quote_cmp);

                if (!mask) {
                    append_to_quote_buffer(p, p->cur, 32);
                    p->cur += 32;
                    continue;
                }

                int pos = __builtin_ctz(mask);

                if (pos > 0) {
                    append_to_quote_buffer(p, p->cur, pos);
                }

                p->cur += pos;

                if (p->cur + 1 < p->end && *(p->cur + 1) == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur += 2;
                    continue;
                }

                yield_quoted_field(p);
                p->state = S_NORMAL;
                p->cur++;
                // Skip delimiter/newline immediately after closing quote
                if (p->cur < p->end && *p->cur == p->delimiter) {
                    p->cur++;
                } else if (p->cur < p->end && *p->cur == '\n') {
                    p->cur++;
                    yield_row(p);
                } else if (p->cur < p->end && *p->cur == '\r' &&
                           p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                    p->cur += 2;
                    yield_row(p);
                }
                p->field_start = p->cur;
                break;
            }
        }
    }

    // Handle remainder with scalar code
    while (p->cur < p->end) {
        uint8_t c = *p->cur++;

        if (p->state == S_NORMAL) {
            if (c == p->delimiter) {
                yield_field(p, p->field_start, p->cur - 1);
                p->field_start = p->cur;
            } else if (c == '\n') {
                const uint8_t *field_end = p->cur - 1;
                if (field_end > p->field_start && *(field_end - 1) == '\r') {
                    field_end--;
                }
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if (c == p->quote && p->cur - 1 == p->field_start) {
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else if (p->state == S_QUOTED) {
            if (c == p->quote) {
                if (p->cur < p->end && *p->cur == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->state = S_NORMAL;
                    // Skip delimiter/newline immediately after closing quote
                    if (p->cur < p->end && *p->cur == p->delimiter) {
                        p->cur++;
                    } else if (p->cur < p->end && *p->cur == '\n') {
                        p->cur++;
                        yield_row(p);
                    } else if (p->cur < p->end && *p->cur == '\r' &&
                               p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                        p->cur += 2;
                        yield_row(p);
                    }
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
            }
        }
    }

    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED) {
        // SECURITY: Report unterminated quote at EOF
        if (p->ecb) {
            p->ecb(p->user, p->line_num, "Unterminated quoted field at EOF");
        }
        // Still yield the partial content so data isn't lost
        if (p->quote_buffer_pos > 0) {
            yield_quoted_field(p);
        }
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

#if !defined(__AVX512F__) && !defined(__AVX2__)
// Scalar fallback using SWAR (SIMD Within A Register) - 1BRC technique
// Processes 8 bytes at a time without SIMD instructions (20-40% faster)
__attribute__((hot))
static void parse_scalar(cisv_parser *p) {
    while (p->cur + 8 <= p->end) {
        if (p->state == S_NORMAL) {
            // Load 8 bytes using memcpy (compiler optimizes this)
            uint64_t word;
            memcpy(&word, p->cur, sizeof(word));

            // SWAR: Check all 8 bytes in parallel
            uint64_t special = swar_has_special(word, p->delimiter, p->quote);

            if (!special) {
                // Fast path: no special chars in 8-byte chunk
                p->cur += 8;
                continue;
            }

            // Process bytes until we find a special character
            while (p->cur < p->end && p->cur < p->field_start + 8) {
                uint8_t c = *p->cur;

                if (c == p->delimiter) {
                    yield_field(p, p->field_start, p->cur);
                    p->cur++;
                    p->field_start = p->cur;
                } else if (c == '\n') {
                    const uint8_t *field_end = p->cur;
                    if (field_end > p->field_start && *(field_end - 1) == '\r') {
                        field_end--;
                    }
                    yield_field(p, p->field_start, field_end);
                    yield_row(p);
                    p->cur++;
                    p->field_start = p->cur;
                } else if (c == p->quote && p->cur == p->field_start) {
                    p->state = S_QUOTED;
                    p->cur++;
                    p->quote_buffer_pos = 0;
                    goto handle_quoted;
                } else {
                    p->cur++;
                }
            }
        } else {
            handle_quoted:
            // Inside quoted field - use SWAR to find closing quote
            while (p->cur + 8 <= p->end) {
                uint64_t word;
                memcpy(&word, p->cur, sizeof(word));

                uint64_t quote_mask = swar_has_byte(word, p->quote);
                if (!quote_mask) {
                    // No quotes in 8-byte chunk - fast copy
                    append_to_quote_buffer(p, p->cur, 8);
                    p->cur += 8;
                    continue;
                }

                // Found a quote - process byte by byte
                int pos = swar_find_first(quote_mask);
                if (pos > 0) {
                    append_to_quote_buffer(p, p->cur, pos);
                }
                p->cur += pos;

                // Check for escaped quote
                if (p->cur + 1 < p->end && *(p->cur + 1) == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur += 2;
                    continue;
                }

                // End of quoted field
                yield_quoted_field(p);
                p->state = S_NORMAL;
                p->cur++;
                // Skip delimiter/newline immediately after closing quote
                if (p->cur < p->end && *p->cur == p->delimiter) {
                    p->cur++;
                } else if (p->cur < p->end && *p->cur == '\n') {
                    p->cur++;
                    yield_row(p);
                } else if (p->cur < p->end && *p->cur == '\r' &&
                           p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                    p->cur += 2;
                    yield_row(p);
                }
                p->field_start = p->cur;
                break;
            }

            // Remainder for quoted field
            while (p->state == S_QUOTED && p->cur < p->end) {
                uint8_t c = *p->cur;

                if (c == p->quote) {
                    if (p->cur + 1 < p->end && *(p->cur + 1) == p->quote) {
                        uint8_t q = p->quote;
                        append_to_quote_buffer(p, &q, 1);
                        p->cur += 2;
                    } else {
                        yield_quoted_field(p);
                        p->state = S_NORMAL;
                        p->cur++;
                        if (p->cur < p->end && *p->cur == p->delimiter) {
                            p->cur++;
                        } else if (p->cur < p->end && *p->cur == '\n') {
                            p->cur++;
                            yield_row(p);
                        } else if (p->cur < p->end && *p->cur == '\r' &&
                                   p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                            p->cur += 2;
                            yield_row(p);
                        }
                        p->field_start = p->cur;
                        break;
                    }
                } else {
                    append_to_quote_buffer(p, p->cur, 1);
                    p->cur++;
                }
            }
        }
    }

    // Handle remainder
    while (p->cur < p->end) {
        uint8_t c = *p->cur++;

        if (p->state == S_NORMAL) {
            if (c == p->delimiter) {
                yield_field(p, p->field_start, p->cur - 1);
                p->field_start = p->cur;
            } else if (c == '\n') {
                const uint8_t *field_end = p->cur - 1;
                if (field_end > p->field_start && *(field_end - 1) == '\r') {
                    field_end--;
                }
                yield_field(p, p->field_start, field_end);
                yield_row(p);
                p->field_start = p->cur;
            } else if (c == p->quote && p->cur - 1 == p->field_start) {
                p->state = S_QUOTED;
                p->quote_buffer_pos = 0;
            }
        } else if (p->state == S_QUOTED) {
            if (c == p->quote) {
                if (p->cur < p->end && *p->cur == p->quote) {
                    uint8_t q = p->quote;
                    append_to_quote_buffer(p, &q, 1);
                    p->cur++;
                } else {
                    yield_quoted_field(p);
                    p->state = S_NORMAL;
                    // Skip delimiter/newline immediately after closing quote
                    if (p->cur < p->end && *p->cur == p->delimiter) {
                        p->cur++;
                    } else if (p->cur < p->end && *p->cur == '\n') {
                        p->cur++;
                        yield_row(p);
                    } else if (p->cur < p->end && *p->cur == '\r' &&
                               p->cur + 1 < p->end && *(p->cur + 1) == '\n') {
                        p->cur += 2;
                        yield_row(p);
                    }
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
            }
        }
    }

    if (p->state == S_NORMAL && p->field_start < p->end) {
        yield_field(p, p->field_start, p->end);
    } else if (p->state == S_QUOTED) {
        // SECURITY: Report unterminated quote at EOF
        if (p->ecb) {
            p->ecb(p->user, p->line_num, "Unterminated quoted field at EOF");
        }
        // Still yield the partial content so data isn't lost
        if (p->quote_buffer_pos > 0) {
            yield_quoted_field(p);
        }
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

    // Allocate quote buffer with NULL check
    p->quote_buffer_size = 4096;
    p->quote_buffer = malloc(p->quote_buffer_size);
    if (!p->quote_buffer) {
        free(p);
        return NULL;
    }
    p->quote_buffer_pos = 0;

    // Allocate stream buffer for streaming mode
    p->stream_buffer_size = 4096;
    p->stream_buffer = malloc(p->stream_buffer_size);
    if (!p->stream_buffer) {
        free(p->quote_buffer);
        free(p);
        return NULL;
    }
    p->stream_buffer_pos = 0;
    p->streaming_mode = false;

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
    if (p->quote_buffer) free(p->quote_buffer);
    if (p->stream_buffer) free(p->stream_buffer);
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

    madvise(p->base, p->size, MADV_SEQUENTIAL | MADV_WILLNEED);

    p->cur = p->base;
    p->end = p->base + p->size;
    p->field_start = p->cur;
    p->state = S_NORMAL;
    p->line_num = 0;
    p->current_row_fields = 0;
    p->quote_buffer_pos = 0;

#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#else
    parse_scalar(p);
#endif

    return 0;
}

// Ultra-fast row counting
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

    size_t count = 0;

#ifdef __AVX512F__
    const __m512i nl = _mm512_set1_epi8('\n');
    size_t i = 0;

    for (; i + 64 <= (size_t)st.st_size; i += 64) {
        __m512i chunk = _mm512_loadu_si512((__m512i*)(base + i));
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, nl);
        count += __builtin_popcountll(mask);
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#elif defined(__AVX2__)
    const __m256i nl = _mm256_set1_epi8('\n');
    size_t i = 0;

    for (; i + 32 <= (size_t)st.st_size; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(base + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, nl);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        count += __builtin_popcount(mask);
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#else
    size_t i = 0;
    for (; i + 8 <= (size_t)st.st_size; i += 8) {
        // Use memcpy for safe unaligned access (optimized by compiler)
        uint64_t word;
        memcpy(&word, base + i, sizeof(word));
        for (int j = 0; j < 8; j++) {
            count += ((word >> (j*8)) & 0xFF) == '\n';
        }
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#endif

    if (st.st_size > 0 && base[st.st_size - 1] != '\n') {
        count++;
    }

    munmap(base, st.st_size);
    close(fd);
    return count;
}

size_t cisv_parser_count_rows_with_config(const char *path, const cisv_config *config) {
    (void)config;
    return cisv_parser_count_rows(path);
}

int cisv_parser_write(cisv_parser *p, const uint8_t *chunk, size_t len) {
    // Enable streaming mode - fields may span chunks
    p->streaming_mode = true;

    p->cur = chunk;
    p->end = chunk + len;
    p->field_start = p->cur;

#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#else
    parse_scalar(p);
#endif

    // After parsing, buffer any partial unquoted field for next chunk
    // (quoted fields are already handled by quote_buffer)
    if (p->state == S_NORMAL && p->field_start && p->field_start < p->cur) {
        // We have a partial field - buffer it for the next write() call
        size_t partial_len = p->cur - p->field_start;
        if (partial_len > 0) {
            append_to_stream_buffer(p, p->field_start, partial_len);
        }
    }

    return 0;
}

void cisv_parser_end(cisv_parser *p) {
    if (p->state == S_NORMAL && p->field_start && p->field_start < p->cur) {
        yield_field(p, p->field_start, p->cur);
    } else if (p->state == S_QUOTED && p->quote_buffer_pos > 0) {
        yield_quoted_field(p);
    }
}

int cisv_parser_get_line_number(const cisv_parser *p) {
    return p ? p->line_num : 0;
}
