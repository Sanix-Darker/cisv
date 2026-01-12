#include "cisv/writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

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
    size_t null_string_len;  // PERF: Cached length for O(1) access

    // State
    int in_field;
    size_t field_count;

    // Statistics
    size_t bytes_written;
    size_t rows_written;
};

// Check if field needs quoting
static inline int needs_quoting(const char *data, size_t len, char delim, char quote) {
#if defined(__AVX512F__) || defined(__AVX2__)
    const uint8_t *cur = (const uint8_t *)data;
    const uint8_t *end = cur + len;

#ifdef __AVX512F__
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
#elif defined(__AVX2__)
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

    while (cur < end) {
        char c = *cur++;
        if (c == delim || c == quote || c == '\r' || c == '\n') {
            return 1;
        }
    }
    return 0;
#else
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == delim || c == quote || c == '\r' || c == '\n') {
            return 1;
        }
    }
    return 0;
#endif
}

static int ensure_buffer_space(cisv_writer *writer, size_t needed) {
    // Check for overflow before addition
    if (needed > SIZE_MAX - writer->buffer_pos) {
        return -2;  // Would overflow
    }
    if (writer->buffer_pos + needed > writer->buffer_size) {
        if (cisv_writer_flush(writer) < 0) {
            return -1;
        }
        if (needed > writer->buffer_size) {
            return -2;
        }
    }
    return 0;
}

static void buffer_write(cisv_writer *writer, const void *data, size_t len) {
    memcpy(writer->buffer + writer->buffer_pos, data, len);
    writer->buffer_pos += len;
}

#ifdef __AVX2__
// SIMD-accelerated quoted field writing
// Scans for quote characters in 32-byte chunks - fast path when no quotes
static int write_quoted_field_simd(cisv_writer *writer, const char *data, size_t len) {
    // Check for overflow: max_size = len * 2 + 2
    if (len > (SIZE_MAX - 2) / 2) {
        return -1;
    }
    size_t max_size = len * 2 + 2;

    int space_result = ensure_buffer_space(writer, max_size);
    if (space_result < 0) {
        return -1;  // Can't fit in buffer, fall back to non-SIMD
    }

    const __m256i quote_v = _mm256_set1_epi8(writer->quote_char);
    uint8_t *out = writer->buffer + writer->buffer_pos;

    // Opening quote
    *out++ = writer->quote_char;

    size_t i = 0;

    // Process 32 bytes at a time
    while (i + 32 <= len) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));
        __m256i quote_cmp = _mm256_cmpeq_epi8(chunk, quote_v);
        uint32_t mask = _mm256_movemask_epi8(quote_cmp);

        if (mask == 0) {
            // Fast path: no quotes in this 32-byte chunk
            _mm256_storeu_si256((__m256i*)out, chunk);
            out += 32;
            i += 32;
        } else {
            // Slow path: process bytes with quotes one at a time
            while (mask && i < len) {
                if (data[i] == writer->quote_char) {
                    *out++ = writer->quote_char;  // Escape quote
                }
                *out++ = data[i++];
                mask >>= 1;
            }
            // Continue with next aligned chunk
            i = (i + 31) & ~31UL;
            if (i > len) i = len;
        }
    }

    // Scalar remainder
    while (i < len) {
        if (data[i] == writer->quote_char) {
            *out++ = writer->quote_char;
        }
        *out++ = data[i++];
    }

    // Closing quote
    *out++ = writer->quote_char;

    writer->buffer_pos = out - writer->buffer;
    return 0;
}
#endif

// PERF: Fallback buffer size for unbuffered writes (4KB for cache efficiency)
#define FALLBACK_BUFFER_SIZE 4096

static int write_quoted_field(cisv_writer *writer, const char *data, size_t len) {
    // Check for overflow: max_size = len * 2 + 2
    if (len > (SIZE_MAX - 2) / 2) {
        return -1;  // Field too large, would overflow
    }
    size_t max_size = len * 2 + 2;

    int space_result = ensure_buffer_space(writer, max_size);
    if (space_result == -2) {
        // PERF: Use local buffer instead of fputc() loop (50-100x faster)
        // The fputc() function has significant per-call overhead
        char fallback_buf[FALLBACK_BUFFER_SIZE];
        size_t fb_pos = 0;

        // Opening quote
        fallback_buf[fb_pos++] = writer->quote_char;

        for (size_t i = 0; i < len; i++) {
            // Flush buffer when nearly full (need room for 2 chars + quote)
            if (fb_pos >= FALLBACK_BUFFER_SIZE - 3) {
                if (fwrite(fallback_buf, 1, fb_pos, writer->output) != fb_pos) {
                    return -1;
                }
                writer->bytes_written += fb_pos;
                fb_pos = 0;
            }

            // Escape quote characters by doubling them
            if (data[i] == writer->quote_char) {
                fallback_buf[fb_pos++] = writer->quote_char;
            }
            fallback_buf[fb_pos++] = data[i];
        }

        // Closing quote
        fallback_buf[fb_pos++] = writer->quote_char;

        // Flush remaining buffer
        if (fb_pos > 0) {
            if (fwrite(fallback_buf, 1, fb_pos, writer->output) != fb_pos) {
                return -1;
            }
            writer->bytes_written += fb_pos;
        }

        return 0;
    } else if (space_result < 0) {
        return -1;
    }

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
    writer->null_string_len = strlen(writer->null_string);  // PERF: Cache length

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

    if (writer->field_count > 0) {
        if (ensure_buffer_space(writer, 1) < 0) return -1;
        writer->buffer[writer->buffer_pos++] = writer->delimiter;
    }

    if (!data) {
        data = writer->null_string;
        len = writer->null_string_len;  // PERF: Use cached length (O(1) vs strlen)
    }

    if (writer->always_quote || needs_quoting(data, len, writer->delimiter, writer->quote_char)) {
#ifdef __AVX2__
        // Use SIMD version for fields >= 64 bytes
        if (len >= 64) {
            int result = write_quoted_field_simd(writer, data, len);
            if (result == 0) goto field_done;
            // Fall through to non-SIMD if SIMD failed (buffer issues)
        }
#endif
        if (write_quoted_field(writer, data, len) < 0) return -1;
    field_done:;
    } else {
        int space_result = ensure_buffer_space(writer, len);
        if (space_result == -2) {
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
