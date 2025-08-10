#ifndef cisv_PARSER_H
#define cisv_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cisv_parser cisv_parser;

typedef void (*cisv_field_cb)(void *user, const char *data, size_t len);
typedef void (*cisv_row_cb)(void *user);

cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user);
void cisv_parser_destroy(cisv_parser *parser);

// Parse whole file (zero‑copy if possible)
int cisv_parser_parse_file(cisv_parser *parser, const char *path);

// Fast counting mode - no callbacks
size_t cisv_parser_count_rows(const char *path);

// Streaming API
int cisv_parser_write(cisv_parser *parser, const uint8_t *chunk, size_t len);
void cisv_parser_end(cisv_parser *parser);

void cisv_parser_set_delimiter(cisv_parser* parser, char delimiter);
void cisv_parser_set_quote(cisv_parser* parser, char quote);
void cisv_parser_set_escape(cisv_parser* parser, char escape);
void cisv_parser_set_skip_empty_lines(cisv_parser* parser, bool skip);
void cisv_parser_set_comment(cisv_parser* parser, char comment);
void cisv_parser_set_trim(cisv_parser* parser, bool trim);
void cisv_parser_set_relaxed(cisv_parser* parser, bool relaxed);
void cisv_parser_set_max_row_size(cisv_parser* parser, size_t max_size);
void cisv_parser_set_from_line(cisv_parser* parser, int from_line);
void cisv_parser_set_to_line(cisv_parser* parser, int to_line);
void cisv_parser_set_skip_lines_with_error(cisv_parser* parser, bool skip);

// Platform-specific defines
#ifdef __linux__
    #define HAS_POSIX_FADVISE 1
    #define HAS_MAP_POPULATE 1
#endif

#ifdef __APPLE__
    #include <sys/types.h>
    #include <sys/sysctl.h>
    // F_RDADVISE for macOS file hints
    #ifdef F_RDADVISE
        #define HAS_RDADVISE 1
    #endif
#endif

// ARM NEON support detection
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define HAS_NEON 1
#endif

#ifdef __cplusplus
}
#endif

#endif // cisv_PARSER_H
