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

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

#if defined(__SSE2__) && !defined(__AVX2__) && !defined(__AVX512F__)
#include <emmintrin.h>
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
// PERF: __restrict hints allow better optimization by promising no aliasing
static inline const uint8_t * __restrict skip_ws_avx2(
    const uint8_t * __restrict start,
    const uint8_t * __restrict end
) {
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
// PERF: __restrict hints allow better optimization by promising no aliasing
static inline const uint8_t * __restrict rskip_ws_avx2(
    const uint8_t * __restrict start,
    const uint8_t * __restrict end
) {
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
// Optimized: 1.5x growth with 64KB minimum increment, cache-line aligned
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

    // 1.5x growth: reduces memory waste from ~50% to ~33%
    size_t grow_size = p->quote_buffer_size + (p->quote_buffer_size >> 1);
    if (grow_size < new_size) grow_size = new_size;
    if (grow_size < MIN_BUFFER_INCREMENT) grow_size = MIN_BUFFER_INCREMENT;
    new_size = grow_size;

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

    // 1.5x growth: reduces memory waste from ~50% to ~33%
    size_t grow_size = p->stream_buffer_size + (p->stream_buffer_size >> 1);
    size_t new_size = (grow_size > required) ? grow_size : required;

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
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
static void parse_neon(cisv_parser *p);
#endif
#if defined(__SSE2__) && !defined(__AVX2__) && !defined(__AVX512F__)
static void parse_sse2(cisv_parser *p);
#endif
// Scalar fallback for platforms without SIMD
static void parse_scalar(cisv_parser *p);

#ifdef __AVX512F__
// AVX-512 ultra-fast path
// PERF: __attribute__((hot)) tells compiler this is frequently called
__attribute__((hot))
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
// PERF: __attribute__((hot)) tells compiler this is frequently called
__attribute__((hot))
static void parse_avx2(cisv_parser *p) {
    const __m256i delim_v = _mm256_set1_epi8(p->delimiter);
    const __m256i quote_v = _mm256_set1_epi8(p->quote);
    const __m256i nl_v = _mm256_set1_epi8('\n');

    while (p->cur + 32 <= p->end) {
        // Multiple prefetch lines for better cache utilization (matches AVX-512 pattern)
        _mm_prefetch(p->cur + PREFETCH_DISTANCE, _MM_HINT_T0);
        _mm_prefetch(p->cur + PREFETCH_DISTANCE + 64, _MM_HINT_T0);
        _mm_prefetch(p->cur + PREFETCH_DISTANCE + 128, _MM_HINT_T0);

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

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
// ARM NEON fast path for Apple Silicon, AWS Graviton, Raspberry Pi
// PERF: __attribute__((hot)) tells compiler this is frequently called
__attribute__((hot))
static void parse_neon(cisv_parser *p) {
    const uint8x16_t delim_v = vdupq_n_u8(p->delimiter);
    const uint8x16_t quote_v = vdupq_n_u8(p->quote);
    const uint8x16_t nl_v = vdupq_n_u8('\n');

    while (p->cur + 16 <= p->end) {
        // Prefetch future data
        __builtin_prefetch(p->cur + PREFETCH_DISTANCE, 0, 1);
        __builtin_prefetch(p->cur + PREFETCH_DISTANCE + 64, 0, 1);

        if (p->state == S_NORMAL) {
            uint8x16_t chunk = vld1q_u8(p->cur);

            // Compare for special characters
            uint8x16_t delim_cmp = vceqq_u8(chunk, delim_v);
            uint8x16_t quote_cmp = vceqq_u8(chunk, quote_v);
            uint8x16_t nl_cmp = vceqq_u8(chunk, nl_v);

            // Combine masks
            uint8x16_t special = vorrq_u8(delim_cmp, vorrq_u8(quote_cmp, nl_cmp));

            // Check if any special chars found (using horizontal max)
            uint8x8_t special_low = vget_low_u8(special);
            uint8x8_t special_high = vget_high_u8(special);
            uint8x8_t special_max = vorr_u8(special_low, special_high);
            uint64_t has_special;
            vst1_u8((uint8_t*)&has_special, special_max);

            if (!has_special) {
                p->cur += 16;
                continue;
            }

            // Process bytes until we find a special character
            while (p->cur + 16 <= p->end) {
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
                    goto handle_quoted_neon;
                } else {
                    p->cur++;
                    if (p->cur >= p->field_start + 16) break;
                }
            }
        } else {
            handle_quoted_neon:
            while (p->cur + 16 <= p->end) {
                uint8x16_t chunk = vld1q_u8(p->cur);
                uint8x16_t quote_cmp = vceqq_u8(chunk, quote_v);

                // Check if any quotes found
                uint8x8_t quote_low = vget_low_u8(quote_cmp);
                uint8x8_t quote_high = vget_high_u8(quote_cmp);
                uint8x8_t quote_max = vorr_u8(quote_low, quote_high);
                uint64_t has_quote;
                vst1_u8((uint8_t*)&has_quote, quote_max);

                if (!has_quote) {
                    append_to_quote_buffer(p, p->cur, 16);
                    p->cur += 16;
                    continue;
                }

                // Find first quote position
                int pos = 0;
                for (int i = 0; i < 16; i++) {
                    if (p->cur[i] == p->quote) {
                        pos = i;
                        break;
                    }
                }

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
        if (p->ecb) {
            p->ecb(p->user, p->line_num, "Unterminated quoted field at EOF");
        }
        if (p->quote_buffer_pos > 0) {
            yield_quoted_field(p);
        }
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

#if defined(__SSE2__) && !defined(__AVX2__) && !defined(__AVX512F__)
// SSE2 fast path for older x86-64 machines (16 bytes at a time)
// PERF: __attribute__((hot)) tells compiler this is frequently called
__attribute__((hot))
static void parse_sse2(cisv_parser *p) {
    const __m128i delim_v = _mm_set1_epi8(p->delimiter);
    const __m128i quote_v = _mm_set1_epi8(p->quote);
    const __m128i nl_v = _mm_set1_epi8('\n');

    while (p->cur + 16 <= p->end) {
        // Prefetch future data
        _mm_prefetch((const char*)(p->cur + PREFETCH_DISTANCE), _MM_HINT_T0);
        _mm_prefetch((const char*)(p->cur + PREFETCH_DISTANCE + 64), _MM_HINT_T0);

        if (p->state == S_NORMAL) {
            __m128i chunk = _mm_loadu_si128((const __m128i*)p->cur);

            __m128i delim_cmp = _mm_cmpeq_epi8(chunk, delim_v);
            __m128i quote_cmp = _mm_cmpeq_epi8(chunk, quote_v);
            __m128i nl_cmp = _mm_cmpeq_epi8(chunk, nl_v);

            __m128i special = _mm_or_si128(delim_cmp, _mm_or_si128(quote_cmp, nl_cmp));
            int mask = _mm_movemask_epi8(special);

            if (!mask) {
                p->cur += 16;
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
                    goto handle_quoted_sse2;
                }

                mask &= mask - 1;
            }

            p->cur += 16;
        } else {
            handle_quoted_sse2:
            while (p->cur + 16 <= p->end) {
                __m128i chunk = _mm_loadu_si128((const __m128i*)p->cur);
                __m128i quote_cmp = _mm_cmpeq_epi8(chunk, quote_v);
                int mask = _mm_movemask_epi8(quote_cmp);

                if (!mask) {
                    append_to_quote_buffer(p, p->cur, 16);
                    p->cur += 16;
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
        if (p->ecb) {
            p->ecb(p->user, p->line_num, "Unterminated quoted field at EOF");
        }
        if (p->quote_buffer_pos > 0) {
            yield_quoted_field(p);
        }
    }

    if (p->current_row_fields > 0) {
        yield_row(p);
    }
}
#endif

// Scalar fallback using SWAR (SIMD Within A Register) - 1BRC technique
// Processes 8 bytes at a time without SIMD instructions (20-40% faster)
// Always compiled as fallback for all platforms
__attribute__((hot, unused))
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

cisv_parser *cisv_parser_create_with_config(const cisv_config *config) {
    if (!config) return NULL;

    // SECURITY: Validate configuration to prevent parsing ambiguities
    // Delimiter cannot be the same as quote character
    if (config->delimiter == config->quote) {
        return NULL;  // Invalid configuration
    }

    // Delimiter cannot be a newline character (would break row detection)
    if (config->delimiter == '\n' || config->delimiter == '\r') {
        return NULL;  // Invalid configuration
    }

    // Quote character cannot be a newline character
    if (config->quote == '\n' || config->quote == '\r') {
        return NULL;  // Invalid configuration
    }

    // Escape character validation (if set)
    if (config->escape != '\0') {
        if (config->escape == '\n' || config->escape == '\r') {
            return NULL;  // Invalid configuration
        }
        if (config->escape == config->delimiter) {
            return NULL;  // Invalid configuration
        }
    }

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

    // Allocate quote buffer with cache-line alignment for SIMD access
    // Start at 64KB to avoid early reallocations and match MIN_BUFFER_INCREMENT
    p->quote_buffer_size = MIN_BUFFER_INCREMENT;
    p->quote_buffer = aligned_alloc(CACHE_LINE_SIZE, p->quote_buffer_size);
    if (!p->quote_buffer) {
        free(p);
        return NULL;
    }
    p->quote_buffer_pos = 0;

    // Allocate stream buffer for streaming mode (cache-line aligned, 64KB)
    p->stream_buffer_size = MIN_BUFFER_INCREMENT;
    p->stream_buffer = aligned_alloc(CACHE_LINE_SIZE, p->stream_buffer_size);
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

    // Dispatch to best available SIMD implementation
#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    parse_neon(p);
#elif defined(__SSE2__)
    parse_sse2(p);
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

    // Unrolled loop: process 64 bytes (2x 32-byte chunks) per iteration
    for (; i + 64 <= (size_t)st.st_size; i += 64) {
        __m256i chunk1 = _mm256_loadu_si256((__m256i*)(base + i));
        __m256i chunk2 = _mm256_loadu_si256((__m256i*)(base + i + 32));
        count += __builtin_popcount(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, nl)));
        count += __builtin_popcount(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, nl)));
    }

    // Handle remaining 32-byte chunk
    for (; i + 32 <= (size_t)st.st_size; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(base + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, nl);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        count += __builtin_popcount(mask);
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    // ARM NEON row counting (16 bytes at a time)
    const uint8x16_t nl = vdupq_n_u8('\n');
    size_t i = 0;

    // Unrolled: process 32 bytes per iteration
    for (; i + 32 <= (size_t)st.st_size; i += 32) {
        uint8x16_t chunk1 = vld1q_u8(base + i);
        uint8x16_t chunk2 = vld1q_u8(base + i + 16);
        uint8x16_t cmp1 = vceqq_u8(chunk1, nl);
        uint8x16_t cmp2 = vceqq_u8(chunk2, nl);
        // Count matches: each 0xFF byte becomes 1 when right-shifted
        count += vaddvq_u8(vshrq_n_u8(cmp1, 7));
        count += vaddvq_u8(vshrq_n_u8(cmp2, 7));
    }

    // Handle remaining 16-byte chunk
    for (; i + 16 <= (size_t)st.st_size; i += 16) {
        uint8x16_t chunk = vld1q_u8(base + i);
        uint8x16_t cmp = vceqq_u8(chunk, nl);
        count += vaddvq_u8(vshrq_n_u8(cmp, 7));
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#elif defined(__SSE2__)
    // SSE2 row counting (16 bytes at a time)
    const __m128i nl = _mm_set1_epi8('\n');
    size_t i = 0;

    // Unrolled: process 32 bytes per iteration
    for (; i + 32 <= (size_t)st.st_size; i += 32) {
        __m128i chunk1 = _mm_loadu_si128((const __m128i*)(base + i));
        __m128i chunk2 = _mm_loadu_si128((const __m128i*)(base + i + 16));
        count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk1, nl)));
        count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk2, nl)));
    }

    // Handle remaining 16-byte chunk
    for (; i + 16 <= (size_t)st.st_size; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i*)(base + i));
        count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, nl)));
    }

    for (; i < (size_t)st.st_size; i++) {
        count += (base[i] == '\n');
    }
#else
    // Scalar fallback using proper SWAR (SIMD Within A Register)
    size_t i = 0;
    for (; i + 8 <= (size_t)st.st_size; i += 8) {
        // Use memcpy for safe unaligned access (optimized by compiler)
        uint64_t word;
        memcpy(&word, base + i, sizeof(word));
        // SWAR: each matching byte has its high bit set in the mask
        uint64_t nl_mask = swar_has_byte(word, '\n');
        // Each match sets 0x80 in that byte position, popcount gives 8x actual count
        count += __builtin_popcountll(nl_mask) >> 3;
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

    // Dispatch to best available SIMD implementation
#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    parse_neon(p);
#elif defined(__SSE2__)
    parse_sse2(p);
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

// =============================================================================
// Parallel Chunk Processing Implementation (1 Billion Row Challenge technique)
// =============================================================================

cisv_mmap_file_t *cisv_mmap_open(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    if (st.st_size == 0) {
        close(fd);
        errno = EINVAL;
        return NULL;
    }

    int flags = MAP_PRIVATE;
#ifdef MAP_POPULATE
    flags |= MAP_POPULATE;
#endif

    uint8_t *data = (uint8_t*)mmap(NULL, st.st_size, PROT_READ, flags, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    // Advise kernel for sequential access
    madvise(data, st.st_size, MADV_SEQUENTIAL | MADV_WILLNEED);

    cisv_mmap_file_t *file = malloc(sizeof(cisv_mmap_file_t));
    if (!file) {
        munmap(data, st.st_size);
        close(fd);
        return NULL;
    }

    file->data = data;
    file->size = st.st_size;
    file->fd = fd;

    return file;
}

void cisv_mmap_close(cisv_mmap_file_t *file) {
    if (!file) return;
    if (file->data) munmap(file->data, file->size);
    if (file->fd >= 0) close(file->fd);
    free(file);
}

cisv_chunk_t *cisv_split_chunks(
    const cisv_mmap_file_t *file,
    int num_chunks,
    int *chunk_count
) {
    if (!file || !file->data || num_chunks <= 0 || !chunk_count) {
        return NULL;
    }

    // Clamp to reasonable chunk count
    if (num_chunks > 256) num_chunks = 256;

    // Calculate approximate chunk size
    size_t chunk_size = file->size / num_chunks;
    if (chunk_size < 4096) {
        // File too small for requested chunks
        num_chunks = 1;
        chunk_size = file->size;
    }

    cisv_chunk_t *chunks = calloc(num_chunks, sizeof(cisv_chunk_t));
    if (!chunks) return NULL;

    const uint8_t *data = file->data;
    const uint8_t *end = file->data + file->size;
    const uint8_t *chunk_start = data;
    int actual_chunks = 0;

    for (int i = 0; i < num_chunks && chunk_start < end; i++) {
        const uint8_t *target_end;

        if (i == num_chunks - 1) {
            // Last chunk gets everything remaining
            target_end = end;
        } else {
            // Find newline near target boundary
            target_end = chunk_start + chunk_size;
            if (target_end > end) target_end = end;

            // Search forward for newline (row boundary)
            while (target_end < end && *target_end != '\n') {
                target_end++;
            }
            if (target_end < end) {
                target_end++;  // Include the newline
            }
        }

        // Count rows in this chunk using SIMD when available
        size_t row_count = 0;
        const uint8_t *p = chunk_start;

#ifdef __AVX512F__
        const __m512i nl_v = _mm512_set1_epi8('\n');
        while (p + 64 <= target_end) {
            __m512i chunk = _mm512_loadu_si512((__m512i*)p);
            __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, nl_v);
            row_count += __builtin_popcountll(mask);
            p += 64;
        }
#elif defined(__AVX2__)
        const __m256i nl_v = _mm256_set1_epi8('\n');
        // Unrolled: process 64 bytes per iteration
        while (p + 64 <= target_end) {
            __m256i chunk1 = _mm256_loadu_si256((__m256i*)p);
            __m256i chunk2 = _mm256_loadu_si256((__m256i*)(p + 32));
            row_count += __builtin_popcount(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, nl_v)));
            row_count += __builtin_popcount(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk2, nl_v)));
            p += 64;
        }
        // Handle remaining 32-byte chunk
        while (p + 32 <= target_end) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)p);
            row_count += __builtin_popcount(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, nl_v)));
            p += 32;
        }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
        // ARM NEON newline counting
        const uint8x16_t nl_v = vdupq_n_u8('\n');
        while (p + 32 <= target_end) {
            uint8x16_t chunk1 = vld1q_u8(p);
            uint8x16_t chunk2 = vld1q_u8(p + 16);
            uint8x16_t cmp1 = vceqq_u8(chunk1, nl_v);
            uint8x16_t cmp2 = vceqq_u8(chunk2, nl_v);
            row_count += vaddvq_u8(vshrq_n_u8(cmp1, 7));
            row_count += vaddvq_u8(vshrq_n_u8(cmp2, 7));
            p += 32;
        }
        while (p + 16 <= target_end) {
            uint8x16_t chunk = vld1q_u8(p);
            uint8x16_t cmp = vceqq_u8(chunk, nl_v);
            row_count += vaddvq_u8(vshrq_n_u8(cmp, 7));
            p += 16;
        }
#elif defined(__SSE2__)
        // SSE2 newline counting
        const __m128i nl_v = _mm_set1_epi8('\n');
        while (p + 32 <= target_end) {
            __m128i chunk1 = _mm_loadu_si128((const __m128i*)p);
            __m128i chunk2 = _mm_loadu_si128((const __m128i*)(p + 16));
            row_count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk1, nl_v)));
            row_count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk2, nl_v)));
            p += 32;
        }
        while (p + 16 <= target_end) {
            __m128i chunk = _mm_loadu_si128((const __m128i*)p);
            row_count += __builtin_popcount(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, nl_v)));
            p += 16;
        }
#else
        // SWAR newline counting
        while (p + 8 <= target_end) {
            uint64_t word;
            memcpy(&word, p, sizeof(word));
            uint64_t nl_mask = swar_has_byte(word, '\n');
            // Each match sets 0x80 in that byte position, popcount gives 8x actual count
            row_count += __builtin_popcountll(nl_mask) >> 3;
            p += 8;
        }
#endif

        // Remainder
        while (p < target_end) {
            if (*p++ == '\n') row_count++;
        }

        chunks[actual_chunks].start = chunk_start;
        chunks[actual_chunks].end = target_end;
        chunks[actual_chunks].row_count = row_count;
        chunks[actual_chunks].chunk_index = actual_chunks;

        chunk_start = target_end;
        actual_chunks++;
    }

    *chunk_count = actual_chunks;
    return chunks;
}

int cisv_parse_chunk(cisv_parser *p, const cisv_chunk_t *chunk) {
    if (!p || !chunk || !chunk->start) return -1;

    // Reset parser state for new chunk
    p->cur = chunk->start;
    p->end = chunk->end;
    p->field_start = p->cur;
    p->state = S_NORMAL;
    p->quote_buffer_pos = 0;
    p->current_row_fields = 0;

    // Dispatch to best available SIMD implementation
#ifdef __AVX512F__
    parse_avx512(p);
#elif defined(__AVX2__)
    parse_avx2(p);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    parse_neon(p);
#elif defined(__SSE2__)
    parse_sse2(p);
#else
    parse_scalar(p);
#endif

    return 0;
}

// =============================================================================
// Batch Parsing Implementation
// High-performance API that returns all data at once (no callbacks)
// =============================================================================

// Initial allocation sizes for batch parsing
#define BATCH_INITIAL_ROWS 1024
#define BATCH_INITIAL_FIELDS 8192
#define BATCH_INITIAL_DATA (1024 * 1024)  // 1MB initial data buffer

// Internal collector for batch parsing
typedef struct {
    cisv_result_t *result;
    size_t current_row_start;  // Index in all_fields where current row starts
} BatchCollector;

// Ensure result has capacity for more rows
static inline bool batch_ensure_rows(cisv_result_t *r, size_t needed) {
    if (r->row_count + needed <= r->row_capacity) return true;

    // 1.5x growth: reduces memory waste from ~50% to ~33%
    size_t required = r->row_count + needed;
    size_t new_cap = r->row_capacity + (r->row_capacity >> 1);
    if (new_cap < required) new_cap = required;

    cisv_row_t *new_rows = realloc(r->rows, new_cap * sizeof(cisv_row_t));
    if (!new_rows) return false;

    r->rows = new_rows;
    r->row_capacity = new_cap;
    return true;
}

// Ensure result has capacity for more fields
static inline bool batch_ensure_fields(cisv_result_t *r, size_t needed) {
    if (r->total_fields + needed <= r->fields_capacity) return true;

    // 1.5x growth: reduces memory waste from ~50% to ~33%
    size_t required = r->total_fields + needed;
    size_t new_cap = r->fields_capacity + (r->fields_capacity >> 1);
    if (new_cap < required) new_cap = required;

    char **new_fields = realloc(r->all_fields, new_cap * sizeof(char*));
    if (!new_fields) return false;

    size_t *new_lengths = realloc(r->all_lengths, new_cap * sizeof(size_t));
    if (!new_lengths) return false;

    r->all_fields = new_fields;
    r->all_lengths = new_lengths;
    r->fields_capacity = new_cap;
    return true;
}

// Ensure result has capacity for more field data
// NOTE: all_fields[] stores offsets (not pointers) during parsing to avoid
// O(n) pointer fixup on reallocation. Offsets are converted to pointers
// in batch_result_finalize() after parsing completes.
static inline bool batch_ensure_data(cisv_result_t *r, size_t needed) {
    if (r->field_data_size + needed <= r->field_data_capacity) return true;

    // 1.5x growth: reduces memory waste from ~50% to ~33%
    size_t required = r->field_data_size + needed;
    size_t new_cap = r->field_data_capacity + (r->field_data_capacity >> 1);
    if (new_cap < required) new_cap = required;

    // Round up to 64-byte alignment for cache efficiency
    new_cap = (new_cap + 63) & ~(size_t)63;

    char *new_data = realloc(r->field_data, new_cap);
    if (!new_data) return false;

    // No pointer fixup needed - we store offsets, not pointers
    r->field_data = new_data;
    r->field_data_capacity = new_cap;
    return true;
}

// Batch field callback - accumulates fields into result
// NOTE: stores field offset (not pointer) to avoid O(n) fixup on realloc
static void batch_field_cb(void *user, const char *data, size_t len) {
    BatchCollector *bc = (BatchCollector *)user;
    cisv_result_t *r = bc->result;

    // Ensure we have space
    if (!batch_ensure_fields(r, 1)) {
        r->error_code = -1;
        snprintf(r->error_message, sizeof(r->error_message), "Out of memory (fields)");
        return;
    }

    if (!batch_ensure_data(r, len + 1)) {
        r->error_code = -1;
        snprintf(r->error_message, sizeof(r->error_message), "Out of memory (data)");
        return;
    }

    // Store current offset before copying (field_data may reallocate)
    size_t offset = r->field_data_size;

    // Copy field data (null-terminated for convenience)
    char *dest = r->field_data + offset;
    memcpy(dest, data, len);
    dest[len] = '\0';

    // Record field offset (converted to pointer in batch_result_finalize)
    r->all_fields[r->total_fields] = (char*)(uintptr_t)offset;
    r->all_lengths[r->total_fields] = len;
    r->total_fields++;
    r->field_data_size += len + 1;
}

// Batch row callback - stores field start index (pointers set up at end)
// We store the start index in the fields pointer and convert to actual
// pointers after parsing is complete (to handle reallocation during parsing)
static void batch_row_cb(void *user) {
    BatchCollector *bc = (BatchCollector *)user;
    cisv_result_t *r = bc->result;

    if (!batch_ensure_rows(r, 1)) {
        r->error_code = -1;
        snprintf(r->error_message, sizeof(r->error_message), "Out of memory (rows)");
        return;
    }

    // Store field count and start index (not actual pointers - those are set later)
    size_t field_count = r->total_fields - bc->current_row_start;
    cisv_row_t *row = &r->rows[r->row_count];

    // Store start index as intptr_t (will be converted to pointer later)
    row->fields = (char **)(intptr_t)bc->current_row_start;
    row->field_lengths = NULL;  // Will be set during finalization
    row->field_count = field_count;

    r->row_count++;
    bc->current_row_start = r->total_fields;
}

// Finalize result by converting stored indices/offsets to actual pointers
// Must be called after all parsing is complete
static void batch_result_finalize(cisv_result_t *r) {
    // First, convert all field offsets to actual pointers
    // This is O(n) but only done once at the end, not on every realloc
    for (size_t i = 0; i < r->total_fields; i++) {
        size_t offset = (size_t)(uintptr_t)r->all_fields[i];
        r->all_fields[i] = r->field_data + offset;
    }

    // Now convert row field indices to actual pointers
    for (size_t i = 0; i < r->row_count; i++) {
        size_t start_index = (size_t)(intptr_t)r->rows[i].fields;
        r->rows[i].fields = &r->all_fields[start_index];
        r->rows[i].field_lengths = &r->all_lengths[start_index];
    }
}

// Batch error callback
static void batch_error_cb(void *user, int line, const char *msg) {
    BatchCollector *bc = (BatchCollector *)user;
    cisv_result_t *r = bc->result;

    if (r->error_code == 0) {
        r->error_code = -1;
        snprintf(r->error_message, sizeof(r->error_message),
                 "Parse error at line %d: %s", line, msg ? msg : "unknown error");
    }
}

// Maximum initial allocation for pre-sized buffers (500MB)
#define BATCH_MAX_INITIAL_ALLOC (500 * 1024 * 1024)

// Allocate and initialize a new result structure with size hints
// file_size_hint: estimated file size (0 to use defaults)
static cisv_result_t *batch_result_create_with_hint(size_t file_size_hint) {
    cisv_result_t *r = calloc(1, sizeof(cisv_result_t));
    if (!r) return NULL;

    // Estimate buffer sizes based on file size
    // Heuristics: ~100 bytes/row average, ~8 fields/row average
    size_t row_cap = BATCH_INITIAL_ROWS;
    size_t field_cap = BATCH_INITIAL_FIELDS;
    size_t data_cap = BATCH_INITIAL_DATA;

    if (file_size_hint > 0) {
        // Estimate rows: assume ~100 bytes per row on average
        size_t est_rows = file_size_hint / 100;
        if (est_rows > row_cap && est_rows * sizeof(cisv_row_t) < BATCH_MAX_INITIAL_ALLOC) {
            row_cap = est_rows;
        }

        // Estimate fields: assume ~8 fields per row
        size_t est_fields = est_rows * 8;
        if (est_fields > field_cap && est_fields * sizeof(char*) < BATCH_MAX_INITIAL_ALLOC) {
            field_cap = est_fields;
        }

        // Field data needs at least file_size bytes (strings + null terminators)
        // Add 10% overhead for null terminators
        size_t est_data = file_size_hint + (file_size_hint / 10);
        if (est_data > data_cap && est_data < BATCH_MAX_INITIAL_ALLOC) {
            data_cap = est_data;
        }

        // Align data capacity to 64 bytes for cache efficiency
        data_cap = (data_cap + 63) & ~(size_t)63;
    }

    // Allocate buffers with estimated sizes
    r->rows = malloc(row_cap * sizeof(cisv_row_t));
    r->all_fields = malloc(field_cap * sizeof(char*));
    r->all_lengths = malloc(field_cap * sizeof(size_t));
    r->field_data = malloc(data_cap);

    if (!r->rows || !r->all_fields || !r->all_lengths || !r->field_data) {
        free(r->rows);
        free(r->all_fields);
        free(r->all_lengths);
        free(r->field_data);
        free(r);
        return NULL;
    }

    r->row_capacity = row_cap;
    r->fields_capacity = field_cap;
    r->field_data_capacity = data_cap;

    return r;
}

// Allocate and initialize a new result structure with default sizes
static cisv_result_t *batch_result_create(void) {
    return batch_result_create_with_hint(0);
}

void cisv_result_free(cisv_result_t *result) {
    if (!result) return;

    // Note: row->fields and row->field_lengths point into all_fields/all_lengths
    // so we don't need to free them separately
    free(result->rows);
    free(result->all_fields);
    free(result->all_lengths);
    free(result->field_data);
    free(result);
}

cisv_result_t *cisv_parse_file_batch(const char *path, const cisv_config *config) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    // Get file size for buffer pre-sizing (reduces reallocations)
    size_t file_size_hint = 0;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        file_size_hint = (size_t)st.st_size;
    }

    cisv_result_t *result = batch_result_create_with_hint(file_size_hint);
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }

    BatchCollector bc = {
        .result = result,
        .current_row_start = 0
    };

    // Create config with batch callbacks
    cisv_config batch_config;
    if (config) {
        batch_config = *config;
    } else {
        cisv_config_init(&batch_config);
    }
    batch_config.field_cb = batch_field_cb;
    batch_config.row_cb = batch_row_cb;
    batch_config.error_cb = batch_error_cb;
    batch_config.user = &bc;

    cisv_parser *parser = cisv_parser_create_with_config(&batch_config);
    if (!parser) {
        cisv_result_free(result);
        errno = ENOMEM;
        return NULL;
    }

    int parse_result = cisv_parser_parse_file(parser, path);
    cisv_parser_destroy(parser);

    if (parse_result < 0) {
        result->error_code = parse_result;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to parse file: %s", strerror(-parse_result));
    }

    // Convert stored indices to actual pointers now that parsing is complete
    batch_result_finalize(result);

    return result;
}

cisv_result_t *cisv_parse_string_batch(const char *data, size_t len, const cisv_config *config) {
    if (!data) {
        errno = EINVAL;
        return NULL;
    }

    cisv_result_t *result = batch_result_create();
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }

    BatchCollector bc = {
        .result = result,
        .current_row_start = 0
    };

    // Create config with batch callbacks
    cisv_config batch_config;
    if (config) {
        batch_config = *config;
    } else {
        cisv_config_init(&batch_config);
    }
    batch_config.field_cb = batch_field_cb;
    batch_config.row_cb = batch_row_cb;
    batch_config.error_cb = batch_error_cb;
    batch_config.user = &bc;

    cisv_parser *parser = cisv_parser_create_with_config(&batch_config);
    if (!parser) {
        cisv_result_free(result);
        errno = ENOMEM;
        return NULL;
    }

    cisv_parser_write(parser, (const uint8_t *)data, len);
    cisv_parser_end(parser);
    cisv_parser_destroy(parser);

    // Convert stored indices to actual pointers now that parsing is complete
    batch_result_finalize(result);

    return result;
}

// =============================================================================
// Parallel Batch Parsing Implementation
// =============================================================================

#include <pthread.h>

// Thread argument for parallel parsing
typedef struct {
    const cisv_chunk_t *chunk;
    const cisv_config *config;
    cisv_result_t *result;
} ParallelParseArg;

// Thread function for parallel parsing
static void *parallel_parse_thread(void *arg) {
    ParallelParseArg *parg = (ParallelParseArg *)arg;

    cisv_result_t *result = batch_result_create();
    if (!result) {
        parg->result = NULL;
        return NULL;
    }

    BatchCollector bc = {
        .result = result,
        .current_row_start = 0
    };

    // Create config with batch callbacks
    cisv_config batch_config;
    if (parg->config) {
        batch_config = *parg->config;
    } else {
        cisv_config_init(&batch_config);
    }
    batch_config.field_cb = batch_field_cb;
    batch_config.row_cb = batch_row_cb;
    batch_config.error_cb = batch_error_cb;
    batch_config.user = &bc;

    cisv_parser *parser = cisv_parser_create_with_config(&batch_config);
    if (!parser) {
        cisv_result_free(result);
        parg->result = NULL;
        return NULL;
    }

    cisv_parse_chunk(parser, parg->chunk);
    cisv_parser_destroy(parser);

    // Convert stored indices to actual pointers now that parsing is complete
    batch_result_finalize(result);

    parg->result = result;
    return NULL;
}

// Get number of available CPUs
static int get_cpu_count(void) {
#ifdef _SC_NPROCESSORS_ONLN
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) return (int)count;
#endif
    return 4;  // Default fallback
}

cisv_result_t **cisv_parse_file_parallel(const char *path, const cisv_config *config,
                                          int num_threads, int *result_count) {
    if (!path || !result_count) {
        errno = EINVAL;
        return NULL;
    }

    *result_count = 0;

    // Auto-detect thread count
    if (num_threads <= 0) {
        num_threads = get_cpu_count();
    }

    // Limit to reasonable maximum
    if (num_threads > 64) num_threads = 64;

    // Memory-map the file
    cisv_mmap_file_t *mmap_file = cisv_mmap_open(path);
    if (!mmap_file) {
        return NULL;
    }

    // Split into chunks
    int chunk_count;
    cisv_chunk_t *chunks = cisv_split_chunks(mmap_file, num_threads, &chunk_count);
    if (!chunks || chunk_count == 0) {
        cisv_mmap_close(mmap_file);
        return NULL;
    }

    // Allocate thread arguments and results array
    ParallelParseArg *args = calloc(chunk_count, sizeof(ParallelParseArg));
    pthread_t *threads = calloc(chunk_count, sizeof(pthread_t));
    cisv_result_t **results = calloc(chunk_count, sizeof(cisv_result_t *));

    if (!args || !threads || !results) {
        free(args);
        free(threads);
        free(results);
        free(chunks);
        cisv_mmap_close(mmap_file);
        errno = ENOMEM;
        return NULL;
    }

    // Launch threads
    for (int i = 0; i < chunk_count; i++) {
        args[i].chunk = &chunks[i];
        args[i].config = config;
        args[i].result = NULL;

        if (pthread_create(&threads[i], NULL, parallel_parse_thread, &args[i]) != 0) {
            // Thread creation failed - wait for already-launched threads
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
                if (args[j].result) {
                    cisv_result_free(args[j].result);
                }
            }
            free(args);
            free(threads);
            free(results);
            free(chunks);
            cisv_mmap_close(mmap_file);
            return NULL;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < chunk_count; i++) {
        pthread_join(threads[i], NULL);
        results[i] = args[i].result;
    }

    free(args);
    free(threads);
    free(chunks);
    cisv_mmap_close(mmap_file);

    *result_count = chunk_count;
    return results;
}

void cisv_results_free(cisv_result_t **results, int count) {
    if (!results) return;

    for (int i = 0; i < count; i++) {
        cisv_result_free(results[i]);
    }
    free(results);
}
