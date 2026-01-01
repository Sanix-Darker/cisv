#include "cisv/parser.h"
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
} cisv_parser;

// Ultra-fast whitespace table with bit manipulation
static const uint64_t ws_table[4] = {
    0x0000000100003e00ULL, // 0-63: space, tab, CR, LF
    0, 0, 0
};

static inline bool is_ws(uint8_t c) {
    return (ws_table[c >> 6] >> (c & 63)) & 1;
}

void cisv_config_init(cisv_config *config) {
    memset(config, 0, sizeof(*config));
    config->delimiter = ',';
    config->quote = '"';
    config->from_line = 1;
}

// Ensure quote buffer has enough space
static inline void ensure_quote_buffer(cisv_parser *p, size_t needed) {
    if (p->quote_buffer_pos + needed > p->quote_buffer_size) {
        size_t new_size = (p->quote_buffer_size + needed) * 2;
        p->quote_buffer = realloc(p->quote_buffer, new_size);
        p->quote_buffer_size = new_size;
    }
}

// Append to quote buffer
static inline void append_to_quote_buffer(cisv_parser *p, const uint8_t *data, size_t len) {
    ensure_quote_buffer(p, len);
    memcpy(p->quote_buffer + p->quote_buffer_pos, data, len);
    p->quote_buffer_pos += len;
}

// Inline hot-path functions
static inline void yield_field(cisv_parser *p, const uint8_t *start, const uint8_t *end) {
    if (!p->fcb) return;

    if (p->trim) {
        while (start < end && is_ws(*start)) start++;
        while (start < end && is_ws(*(end-1))) end--;
    }

    if (start < end || !p->skip_empty_lines) {
        p->fcb(p->user, (const char*)start, end - start);
        p->fields++;
        p->current_row_fields++;
    }
}

// Yield field from quote buffer
static inline void yield_quoted_field(cisv_parser *p) {
    if (!p->fcb) return;

    const uint8_t *start = p->quote_buffer;
    const uint8_t *end = p->quote_buffer + p->quote_buffer_pos;

    if (p->trim) {
        while (start < end && is_ws(*start)) start++;
        while (start < end && is_ws(*(end-1))) end--;
    }

    if (start < end || !p->skip_empty_lines) {
        p->fcb(p->user, (const char*)start, end - start);
        p->fields++;
        p->current_row_fields++;
    }

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
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
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
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
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
// Scalar fallback with aggressive unrolling
static void parse_scalar(cisv_parser *p) {
    while (p->cur + 8 <= p->end) {
        if (p->state == S_NORMAL) {
            uint64_t word = *(uint64_t*)p->cur;

            uint64_t has_delim = 0, has_nl = 0, has_quote = 0;
            for (int i = 0; i < 8; i++) {
                uint8_t c = (word >> (i*8)) & 0xFF;
                has_delim |= (c == p->delimiter) << i;
                has_nl |= (c == '\n') << i;
                has_quote |= (c == p->quote) << i;
            }

            if (!(has_delim | has_nl | has_quote)) {
                p->cur += 8;
                continue;
            }

            for (int i = 0; i < 8; i++) {
                uint8_t c = (word >> (i*8)) & 0xFF;

                if (c == p->delimiter) {
                    yield_field(p, p->field_start, p->cur + i);
                    p->field_start = p->cur + i + 1;
                } else if (c == '\n') {
                    const uint8_t *field_end = p->cur + i;
                    if (field_end > p->field_start && *(field_end - 1) == '\r') {
                        field_end--;
                    }
                    yield_field(p, p->field_start, field_end);
                    yield_row(p);
                    p->field_start = p->cur + i + 1;
                } else if (c == p->quote && p->cur + i == p->field_start) {
                    p->state = S_QUOTED;
                    p->cur += i + 1;
                    p->quote_buffer_pos = 0;
                    goto handle_quoted;
                }
            }

            p->cur += 8;
        } else {
            handle_quoted:
            while (p->cur < p->end) {
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
                    p->field_start = p->cur;
                }
            } else {
                append_to_quote_buffer(p, p->cur - 1, 1);
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

    p->quote_buffer_size = 4096;
    p->quote_buffer = malloc(p->quote_buffer_size);
    p->quote_buffer_pos = 0;

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
        uint64_t word = *(uint64_t*)(base + i);
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
    p->cur = chunk;
    p->end = chunk + len;

    if (p->state == S_NORMAL && p->current_row_fields == 0) {
        p->field_start = p->cur;
    } else {
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
}

int cisv_parser_get_line_number(const cisv_parser *p) {
    return p ? p->line_num : 0;
}
