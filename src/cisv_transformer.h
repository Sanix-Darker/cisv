#ifndef CISV_TRANSFORMER_H
#define CISV_TRANSFORMER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Transform types
typedef enum {
    TRANSFORM_NONE = 0,

    // String transforms
    TRANSFORM_UPPERCASE,
    TRANSFORM_LOWERCASE,
    TRANSFORM_TRIM,
    TRANSFORM_TRIM_LEFT,
    TRANSFORM_TRIM_RIGHT,

    // Type conversions
    TRANSFORM_TO_INT,
    TRANSFORM_TO_FLOAT,
    TRANSFORM_TO_BOOL,

    // Crypto transforms
    TRANSFORM_HASH_MD5,
    TRANSFORM_HASH_SHA256,
    TRANSFORM_ENCRYPT_AES256,
    TRANSFORM_DECRYPT_AES256,

    // Data transforms
    TRANSFORM_BASE64_ENCODE,
    TRANSFORM_BASE64_DECODE,
    TRANSFORM_URL_ENCODE,
    TRANSFORM_URL_DECODE,

    // Custom/JS callback
    TRANSFORM_CUSTOM_JS,

    TRANSFORM_MAX
} cisv_transform_type_t;

// Transform result
typedef struct {
    char *data;
    size_t len;
    int needs_free;  // Whether data needs to be freed
} cisv_transform_result_t;

// Transform context (for crypto operations)
typedef struct {
    void *key;
    size_t key_len;
    void *iv;
    size_t iv_len;
    void *extra;  // For any additional data
} cisv_transform_context_t;

// Transform function signature
typedef cisv_transform_result_t (*cisv_transform_fn)(
    const char *data,
    size_t len,
    cisv_transform_context_t *ctx
);

// Transform pipeline entry
typedef struct {
    cisv_transform_type_t type;
    cisv_transform_fn fn;
    cisv_transform_context_t *ctx;
    int field_index;  // -1 for all fields
    void *js_callback;  // For JS callbacks (napi_ref)
} cisv_transform_t;

// Transform pipeline
typedef struct {
    cisv_transform_t *transforms;
    size_t count;
    size_t capacity;

    // Memory pool for transformations
    char *buffer_pool;
    size_t pool_size;
    size_t pool_used;

    // SIMD alignment
    size_t alignment;
} cisv_transform_pipeline_t;

typedef struct cisv_js_callback {
    void* env;           // napi_env
    void* callback;      // napi_ref to the JavaScript function
    void* instance;      // napi_ref to the parser instance (for 'this' context)
} cisv_js_callback_t;

// Create/destroy pipeline
cisv_transform_pipeline_t *cisv_transform_pipeline_create(size_t initial_capacity);
void cisv_transform_pipeline_destroy(cisv_transform_pipeline_t *pipeline);

// Add transforms to pipeline
int cisv_transform_pipeline_add(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    cisv_transform_type_t type,
    cisv_transform_context_t *ctx
);

int cisv_transform_pipeline_add_js(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    void *js_callback
);

// Apply transforms
cisv_transform_result_t cisv_transform_apply(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    const char *data,
    size_t len
);

// Built-in transform functions
cisv_transform_result_t cisv_transform_uppercase(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_lowercase(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_trim(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_to_int(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_to_float(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_hash_sha256(const char *data, size_t len, cisv_transform_context_t *ctx);
cisv_transform_result_t cisv_transform_base64_encode(const char *data, size_t len, cisv_transform_context_t *ctx);

void cisv_transform_result_free(cisv_transform_result_t *result);

// SIMD-optimized transforms
#ifdef __AVX2__
void cisv_transform_uppercase_simd(char *dst, const char *src, size_t len);
void cisv_transform_lowercase_simd(char *dst, const char *src, size_t len);
#endif

#ifdef __cplusplus
}
#endif

#endif // CISV_TRANSFORMER_H
