#ifndef CISV_WRITER_H
#define CISV_WRITER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cisv_writer cisv_writer;

// Writer configuration
typedef struct {
    char delimiter;
    char quote_char;
    int always_quote;
    int use_crlf;
    const char *null_string;
    size_t buffer_size;
} cisv_writer_config;

// Create writer with default config
cisv_writer *cisv_writer_create(FILE *output);

// Create writer with custom config
cisv_writer *cisv_writer_create_config(FILE *output, const cisv_writer_config *config);

// Destroy writer and flush remaining data
void cisv_writer_destroy(cisv_writer *writer);

// Write a single field
int cisv_writer_field(cisv_writer *writer, const char *data, size_t len);

// Write a field from null-terminated string
int cisv_writer_field_str(cisv_writer *writer, const char *str);

// Write a numeric field
int cisv_writer_field_int(cisv_writer *writer, int64_t value);
int cisv_writer_field_double(cisv_writer *writer, double value, int precision);

// End current row
int cisv_writer_row_end(cisv_writer *writer);

// Write complete row from array
int cisv_writer_row(cisv_writer *writer, const char **fields, size_t count);

// Flush buffer to output
int cisv_writer_flush(cisv_writer *writer);

// Get statistics
size_t cisv_writer_bytes_written(const cisv_writer *writer);
size_t cisv_writer_rows_written(const cisv_writer *writer);

#ifdef __cplusplus
}
#endif

#endif // CISV_WRITER_H
