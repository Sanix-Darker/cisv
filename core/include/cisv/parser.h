#ifndef CISV_PARSER_H
#define CISV_PARSER_H

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

// =============================================================================
// Parallel Chunk Processing API (1 Billion Row Challenge technique)
// Enables multi-threaded parsing with near-linear scaling
// =============================================================================

// Chunk structure for parallel processing
typedef struct {
    const uint8_t *start;    // Start of chunk data
    const uint8_t *end;      // End of chunk data (exclusive)
    size_t row_count;        // Number of complete rows in chunk
    int chunk_index;         // Chunk index for ordering results
} cisv_chunk_t;

// Memory-mapped file handle for chunk processing
typedef struct {
    uint8_t *data;           // Memory-mapped file data
    size_t size;             // Total file size
    int fd;                  // File descriptor
} cisv_mmap_file_t;

// Open file for parallel processing (memory-maps the file)
// Returns NULL on failure, sets errno
cisv_mmap_file_t *cisv_mmap_open(const char *path);

// Close memory-mapped file
void cisv_mmap_close(cisv_mmap_file_t *file);

// Split file into chunks for parallel processing
// Chunks are split at row boundaries (newlines)
// Returns array of chunks (caller must free), sets *chunk_count
// num_chunks: desired number of chunks (typically = number of threads)
cisv_chunk_t *cisv_split_chunks(
    const cisv_mmap_file_t *file,
    int num_chunks,
    int *chunk_count
);

// Parse a single chunk (thread-safe)
// Each thread should have its own parser instance
// Returns 0 on success, negative on error
int cisv_parse_chunk(cisv_parser *parser, const cisv_chunk_t *chunk);

// =============================================================================
// Batch Parsing API (High-Performance Python Bindings)
// Eliminates per-field callbacks by returning all data at once
// =============================================================================

// Single row structure
typedef struct {
    char **fields;           // Array of field pointers (into field_data)
    size_t *field_lengths;   // Length of each field
    size_t field_count;      // Number of fields in this row
} cisv_row_t;

// Complete parsing result
typedef struct {
    cisv_row_t *rows;        // Array of rows
    size_t row_count;        // Number of rows
    size_t row_capacity;     // Allocated capacity for rows
    char *field_data;        // Contiguous field storage (all strings)
    size_t field_data_size;  // Current size of field_data
    size_t field_data_capacity; // Allocated capacity for field_data
    char **all_fields;       // Flat array of all field pointers
    size_t *all_lengths;     // Flat array of all field lengths
    size_t total_fields;     // Total number of fields
    size_t fields_capacity;  // Allocated capacity for fields
    int error_code;          // 0 = success, negative = error
    char error_message[256]; // Error description
} cisv_result_t;

// Parse entire file and return all data at once
// Returns NULL on failure (check errno), caller must free with cisv_result_free()
cisv_result_t *cisv_parse_file_batch(const char *path, const cisv_config *config);

// Parse string buffer and return all data at once
// Returns NULL on failure (check errno), caller must free with cisv_result_free()
cisv_result_t *cisv_parse_string_batch(const char *data, size_t len, const cisv_config *config);

// Free result allocated by cisv_parse_file_batch or cisv_parse_string_batch
void cisv_result_free(cisv_result_t *result);

// =============================================================================
// Parallel Batch Parsing API
// Uses multiple threads for maximum throughput on large files
// =============================================================================

// Parse file in parallel using multiple threads
// Returns array of results (one per chunk), caller must free with cisv_results_free()
// num_threads: number of threads to use (0 = auto-detect CPU count)
// result_count: output parameter for number of results
cisv_result_t **cisv_parse_file_parallel(const char *path, const cisv_config *config,
                                          int num_threads, int *result_count);

// Free array of results from cisv_parse_file_parallel
void cisv_results_free(cisv_result_t **results, int count);

// =============================================================================
// Platform-specific defines
// =============================================================================

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

#endif // CISV_PARSER_H
