#include "cisv/writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

static int write_quoted_field(cisv_writer *writer, const char *data, size_t len) {
    size_t max_size = len * 2 + 2;

    int space_result = ensure_buffer_space(writer, max_size);
    if (space_result == -2) {
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

    if (writer->field_count > 0) {
        if (ensure_buffer_space(writer, 1) < 0) return -1;
        writer->buffer[writer->buffer_pos++] = writer->delimiter;
    }

    if (!data) {
        data = writer->null_string;
        len = strlen(data);
    }

    if (writer->always_quote || needs_quoting(data, len, writer->delimiter, writer->quote_char)) {
        if (write_quoted_field(writer, data, len) < 0) return -1;
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
