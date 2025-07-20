#ifndef cisv_PARSER_H
#define cisv_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cisv_parser cisv_parser;

typedef void (*cisv_field_cb)(void *user, const char *data, size_t len);
typedef void (*cisv_row_cb)(void *user);

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user);
void cisv_parser_destroy(cisv_parser *p);

// Parse whole file (zero‑copy if possible)
int cisv_parser_parse_file(cisv_parser *p, const char *path);

// Streaming API
int cisv_parser_write(cisv_parser *p, const uint8_t *chunk, size_t len);
void cisv_parser_end(cisv_parser *p);

// TODO: Parallel processing (threads)

#ifdef __cplusplus
}
#endif

#endif // cisv_PARSER_H
