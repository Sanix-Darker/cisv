#ifndef cisv_PARSER_H
#define cisv_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cisv_parser cisv_parser;
typedef struct cisv_config cisv_config;

typedef void (*cisv_field_cb)(void *user, const char *data, size_t len);
typedef void (*cisv_row_cb)(void *user);
typedef void (*cisv_error_cb)(void *user, int line, const char *msg);

// Configuration structure for parser initialization
typedef struct cisv_config {
    // Configuration options
    char delimiter;              // field delimiter character (default ',')
    char quote;                  // quote character (default '"')
    char escape;                 // escape character (0 means use RFC4180-style "" escaping)
    bool skip_empty_lines;       // whether to skip empty lines
    char comment;                // comment character (0 means no comments)
    bool trim;                   // whether to trim whitespace from fields
    bool relaxed;                // whether to use relaxed parsing rules
    size_t max_row_size;         // maximum allowed row size (0 = unlimited)
    int from_line;               // start parsing from this line number (1-based)
    int to_line;                 // stop parsing at this line number (0 = until end)
    bool skip_lines_with_error;  // whether to skip lines that cause errors

    // Callbacks
    cisv_field_cb field_cb;      // field callback
    cisv_row_cb row_cb;          // row callback
    cisv_error_cb error_cb;      // error callback (optional)
    void *user;                  // user data passed to callbacks
} cisv_config;

// Initialize config with defaults
void cisv_config_init(cisv_config *config);

// Create parser with configuration
cisv_parser *cisv_parser_create_with_config(const cisv_config *config);

// Legacy API for backward compatibility
cisv_parser *cisv_parser_create(cisv_field_cb fcb, cisv_row_cb rcb, void *user);

void cisv_parser_destroy(cisv_parser *parser);

// Parse whole file (zero‑copy if possible)
int cisv_parser_parse_file(cisv_parser *parser, const char *path);

// Fast counting mode - no callbacks
size_t cisv_parser_count_rows(const char *path);
size_t cisv_parser_count_rows_with_config(const char *path, const cisv_config *config);

// Streaming API
int cisv_parser_write(cisv_parser *parser, const uint8_t *chunk, size_t len);
void cisv_parser_end(cisv_parser *parser);

// Get current line number
int cisv_parser_get_line_number(const cisv_parser *parser);

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
