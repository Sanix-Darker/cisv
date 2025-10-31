#include "cisv_transformer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

#define TRANSFORM_POOL_SIZE (1 << 20)  // 1MB default pool
#define SIMD_ALIGNMENT 64

// Create transform pipeline
cisv_transform_pipeline_t *cisv_transform_pipeline_create(size_t initial_capacity) {
    cisv_transform_pipeline_t *pipeline = calloc(1, sizeof(*pipeline));
    if (!pipeline) return NULL;

    pipeline->capacity = initial_capacity > 0 ? initial_capacity : 16;
    pipeline->transforms = calloc(pipeline->capacity, sizeof(cisv_transform_t));
    if (!pipeline->transforms) {
        free(pipeline);
        return NULL;
    }

    // Allocate aligned memory pool for transformations
    pipeline->pool_size = TRANSFORM_POOL_SIZE;
    pipeline->alignment = SIMD_ALIGNMENT;

#ifdef _WIN32
    pipeline->buffer_pool = _aligned_malloc(pipeline->pool_size, pipeline->alignment);
#else
    if (posix_memalign((void**)&pipeline->buffer_pool, pipeline->alignment, pipeline->pool_size) != 0) {
        pipeline->buffer_pool = NULL;
    }
#endif

    if (!pipeline->buffer_pool) {
        free(pipeline->transforms);
        free(pipeline);
        return NULL;
    }

    return pipeline;
}

// Destroy pipeline
void cisv_transform_pipeline_destroy(cisv_transform_pipeline_t *pipeline) {
    if (!pipeline) return;

    // Free transform contexts
    for (size_t i = 0; i < pipeline->count; i++) {
        if (pipeline->transforms[i].ctx) {
            if (pipeline->transforms[i].ctx->key) {
                // Clear sensitive data before freeing
                memset(pipeline->transforms[i].ctx->key, 0, pipeline->transforms[i].ctx->key_len);
                free(pipeline->transforms[i].ctx->key);
                pipeline->transforms[i].ctx->key = NULL;
            }
            if (pipeline->transforms[i].ctx->iv) {
                // Clear sensitive data before freeing
                memset(pipeline->transforms[i].ctx->iv, 0, pipeline->transforms[i].ctx->iv_len);
                free(pipeline->transforms[i].ctx->iv);
                pipeline->transforms[i].ctx->iv = NULL;
            }
            if (pipeline->transforms[i].ctx->extra) {
                free(pipeline->transforms[i].ctx->extra);
                pipeline->transforms[i].ctx->extra = NULL;
            }
            free(pipeline->transforms[i].ctx);
            pipeline->transforms[i].ctx = NULL;
        }
    }

    free(pipeline->transforms);
    pipeline->transforms = NULL;

#ifdef _WIN32
    _aligned_free(pipeline->buffer_pool);
#else
    free(pipeline->buffer_pool);
#endif
    pipeline->buffer_pool = NULL;

    free(pipeline);
}

// Get transform function for type
static cisv_transform_fn get_transform_function(cisv_transform_type_t type) {
    switch (type) {
        case TRANSFORM_UPPERCASE: return cisv_transform_uppercase;
        case TRANSFORM_LOWERCASE: return cisv_transform_lowercase;
        case TRANSFORM_TRIM: return cisv_transform_trim;
        case TRANSFORM_TO_INT: return cisv_transform_to_int;
        case TRANSFORM_TO_FLOAT: return cisv_transform_to_float;
        case TRANSFORM_HASH_SHA256: return cisv_transform_hash_sha256;
        case TRANSFORM_BASE64_ENCODE: return cisv_transform_base64_encode;
        default: return NULL;
    }
}

// Add transform to pipeline
int cisv_transform_pipeline_add(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    cisv_transform_type_t type,
    cisv_transform_context_t *ctx
) {
    if (!pipeline || type >= TRANSFORM_MAX) return -1;

    // Grow array if needed
    if (pipeline->count >= pipeline->capacity) {
        size_t new_capacity = pipeline->capacity * 2;
        cisv_transform_t *new_transforms = realloc(
            pipeline->transforms,
            new_capacity * sizeof(cisv_transform_t)
        );
        if (!new_transforms) return -1;

        // Zero-initialize new memory
        memset(new_transforms + pipeline->capacity, 0,
               (new_capacity - pipeline->capacity) * sizeof(cisv_transform_t));

        pipeline->transforms = new_transforms;
        pipeline->capacity = new_capacity;
    }

    cisv_transform_t *t = &pipeline->transforms[pipeline->count];
    t->type = type;
    t->field_index = field_index;
    t->fn = get_transform_function(type);
    t->ctx = ctx;
    t->js_callback = NULL;

    pipeline->count++;
    return 0;
}

// Add JS callback transform
int cisv_transform_pipeline_add_js(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    void *js_callback
) {
    if (!pipeline || !js_callback) return -1;

    if (pipeline->count >= pipeline->capacity) {
        size_t new_capacity = pipeline->capacity * 2;
        cisv_transform_t *new_transforms = realloc(
            pipeline->transforms,
            new_capacity * sizeof(cisv_transform_t)
        );
        if (!new_transforms) return -1;

        // Zero-initialize new memory
        memset(new_transforms + pipeline->capacity, 0,
               (new_capacity - pipeline->capacity) * sizeof(cisv_transform_t));

        pipeline->transforms = new_transforms;
        pipeline->capacity = new_capacity;
    }

    cisv_transform_t *t = &pipeline->transforms[pipeline->count];
    t->type = TRANSFORM_CUSTOM_JS;
    t->field_index = field_index;
    t->fn = NULL;
    t->ctx = NULL;
    t->js_callback = js_callback;

    pipeline->count++;
    return 0;
}

// Set header fields for name-based transforms
int cisv_transform_pipeline_set_header(
    cisv_transform_pipeline_t *pipeline,
    const char **field_names,
    size_t field_count
) {
    if (!pipeline || !field_names || field_count == 0) return -1;

    // Free existing header fields if any
    if (pipeline->header_fields) {
        for (size_t i = 0; i < pipeline->header_count; i++) {
            free(pipeline->header_fields[i]);
        }
        free(pipeline->header_fields);
    }

    // Allocate new header fields array
    pipeline->header_fields = malloc(field_count * sizeof(char *));
    if (!pipeline->header_fields) return -1;

    // Copy each field name
    for (size_t i = 0; i < field_count; i++) {
        pipeline->header_fields[i] = strdup(field_names[i]);
        if (!pipeline->header_fields[i]) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                free(pipeline->header_fields[j]);
            }
            free(pipeline->header_fields);
            pipeline->header_fields = NULL;
            return -1;
        }
    }

    pipeline->header_count = field_count;
    return 0;
}

// Add JavaScript callback transform by field name
int cisv_transform_pipeline_add_js_by_name(
    cisv_transform_pipeline_t *pipeline,
    const char *field_name,
    void *js_callback
) {
    if (!pipeline || !field_name || !js_callback || !pipeline->header_fields) return -1;

    // Find field index by name
    int field_index = -1;
    for (size_t i = 0; i < pipeline->header_count; i++) {
        if (strcmp(pipeline->header_fields[i], field_name) == 0) {
            field_index = (int)i;
            break;
        }
    }

    if (field_index == -1) return -1; // Field not found

    return cisv_transform_pipeline_add_js(pipeline, field_index, js_callback);
}

// Add transform by field name
int cisv_transform_pipeline_add_by_name(
    cisv_transform_pipeline_t *pipeline,
    const char *field_name,
    cisv_transform_type_t type,
    cisv_transform_context_t *ctx
) {
    if (!pipeline || !field_name || !pipeline->header_fields) return -1;

    // Find field index by name
    int field_index = -1;
    for (size_t i = 0; i < pipeline->header_count; i++) {
        if (strcmp(pipeline->header_fields[i], field_name) == 0) {
            field_index = (int)i;
            break;
        }
    }

    if (field_index == -1) return -1; // Field not found

    return cisv_transform_pipeline_add(pipeline, field_index, type, ctx);
}


// Apply transforms for a field
cisv_transform_result_t cisv_transform_apply(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    const char *data,
    size_t len
) {
    cisv_transform_result_t result = {
        .data = (char*)data,
        .len = len,
        .needs_free = 0
    };

    if (!pipeline || pipeline->count == 0) {
        return result;
    }

    // Keep track of allocated memory to free
    char *prev_allocated = NULL;

    // Apply all matching transforms
    for (size_t i = 0; i < pipeline->count; i++) {
        cisv_transform_t *t = &pipeline->transforms[i];

        // Skip if not matching field
        if (t->field_index != -1 && t->field_index != field_index) {
            continue;
        }

        // Apply transform
        if (t->fn) {
            cisv_transform_result_t new_result = t->fn(result.data, result.len, t->ctx);

            // Free the previous result if it was allocated and different from new result
            if (result.needs_free && result.data != data && result.data != new_result.data) {
                prev_allocated = result.data;
            }

            result = new_result;

            // Now free the previous allocation if needed
            if (prev_allocated) {
                free(prev_allocated);
                prev_allocated = NULL;
            }
        } else if (t->js_callback) {
            // JS callback handling - to be implemented
            continue;
        }
    }

    return result;
}

// Clean up transform result
void cisv_transform_result_free(cisv_transform_result_t *result) {
    if (result && result->needs_free && result->data) {
        free(result->data);
        result->data = NULL;
        result->needs_free = 0;
        result->len = 0;
    }
}

// SIMD uppercase transform
#ifdef __AVX2__
void cisv_transform_uppercase_simd(char *dst, const char *src, size_t len) {
    const __m256i lower_a = _mm256_set1_epi8('a');
    const __m256i lower_z = _mm256_set1_epi8('z');
    const __m256i diff = _mm256_set1_epi8('a' - 'A');

    size_t i = 0;

    // Process 32 bytes at a time
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(src + i));

        // Check if characters are lowercase
        __m256i is_lower = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(lower_a, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(lower_z, _mm256_set1_epi8(1)), chunk)
        );

        // Convert to uppercase
        __m256i upper = _mm256_sub_epi8(chunk, _mm256_and_si256(is_lower, diff));

        _mm256_storeu_si256((__m256i*)(dst + i), upper);
    }

    // Handle remaining bytes
    for (; i < len; i++) {
        dst[i] = toupper((unsigned char)src[i]);
    }
}
#endif

// Transform implementations
cisv_transform_result_t cisv_transform_uppercase(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result = {0};

    result.data = malloc(len + 1);
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

#ifdef __AVX2__
    cisv_transform_uppercase_simd(result.data, data, len);
#else
    for (size_t i = 0; i < len; i++) {
        result.data[i] = toupper((unsigned char)data[i]);
    }
#endif

    result.data[len] = '\0';
    result.len = len;
    result.needs_free = 1;
    return result;
}

cisv_transform_result_t cisv_transform_lowercase(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;
    result.data = malloc(len + 1);
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

#ifdef __AVX2__
    // Similar SIMD implementation but with uppercase to lowercase
    const __m256i upper_A = _mm256_set1_epi8('A');
    const __m256i upper_Z = _mm256_set1_epi8('Z');
    const __m256i diff = _mm256_set1_epi8('a' - 'A');

    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));

        __m256i is_upper = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(upper_A, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(upper_Z, _mm256_set1_epi8(1)), chunk)
        );

        __m256i lower = _mm256_add_epi8(chunk, _mm256_and_si256(is_upper, diff));

        _mm256_storeu_si256((__m256i*)(result.data + i), lower);
    }

    for (; i < len; i++) {
        result.data[i] = tolower((unsigned char)data[i]);
    }
#else
    for (size_t i = 0; i < len; i++) {
        result.data[i] = tolower((unsigned char)data[i]);
    }
#endif

    result.data[len] = '\0';
    result.len = len;
    result.needs_free = 1;
    return result;
}

cisv_transform_result_t cisv_transform_trim(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    // Find start and end of non-whitespace
    size_t start = 0;
    size_t end = len;

    while (start < len && isspace((unsigned char)data[start])) {
        start++;
    }

    while (end > start && isspace((unsigned char)data[end - 1])) {
        end--;
    }

    cisv_transform_result_t result;
    result.len = end - start;

    // Always allocate new memory to avoid pointer confusion
    result.data = malloc(result.len + 1);
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    if (result.len > 0) {
        memcpy(result.data, data + start, result.len);
    }
    result.data[result.len] = '\0';
    result.needs_free = 1;

    return result;
}

cisv_transform_result_t cisv_transform_to_int(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;

    // Parse integer
    char *temp = malloc(len + 1);
    if (!temp) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    memcpy(temp, data, len);
    temp[len] = '\0';

    char *endptr;
    long long value = strtoll(temp, &endptr, 10);
    free(temp);

    // Format back to string
    result.data = malloc(32);  // Enough for any int64
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    int written = snprintf(result.data, 32, "%lld", value);
    result.len = (written > 0) ? (size_t)written : 0;
    result.needs_free = 1;
    return result;
}

cisv_transform_result_t cisv_transform_to_float(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;

    // Parse float
    char *temp = malloc(len + 1);
    if (!temp) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    memcpy(temp, data, len);
    temp[len] = '\0';

    char *endptr;
    double value = strtod(temp, &endptr);
    free(temp);

    // Format back to string
    result.data = malloc(64);  // Enough for any double
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    int written = snprintf(result.data, 64, "%.6f", value);
    result.len = (written > 0) ? (size_t)written : 0;
    result.needs_free = 1;
    return result;
}

// SHA256 implementation
cisv_transform_result_t cisv_transform_hash_sha256(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;
    result.data = malloc(128);  // Increased buffer size to avoid truncation
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    // FIXME: This is not a safe implementation,
    // will be updated later
    // For now, just return a mock hash
    int written = snprintf(result.data, 128, "sha256_%016lx%016lx%016lx%016lx",
             (unsigned long)len,
             (unsigned long)(len * 0x1234567890ABCDEF),
             (unsigned long)(len * 0xFEDCBA0987654321),
             (unsigned long)(len * 0xDEADBEEFC0FFEE00));

    result.len = (written > 0 && written < 128) ? (size_t)written : 64;
    result.needs_free = 1;
    return result;
}

// Base64 encoding
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

cisv_transform_result_t cisv_transform_base64_encode(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;

    // Calculate output size
    size_t out_len = ((len + 2) / 3) * 4;
    result.data = malloc(out_len + 1);
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    size_t i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                result.data[j++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(size_t k = i; k < 3; k++)
            char_array_3[k] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (size_t k = 0; k < i + 1; k++)
            result.data[j++] = base64_chars[char_array_4[k]];

        while(i++ < 3)
            result.data[j++] = '=';
    }

    result.data[j] = '\0';
    result.len = j;
    result.needs_free = 1;
    return result;
}
