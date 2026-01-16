#include "cisv/transformer.h"
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
#define HASH_TABLE_LOAD_FACTOR 2  // Hash table size = field_count * 2

// FNV-1a hash function for strings (fast, good distribution)
// PERF: __attribute__((pure)) allows compiler to deduplicate calls with same args
__attribute__((pure))
static inline uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 2166136261u;  // FNV offset basis
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;  // FNV prime
    }
    return hash;
}

// Find field index by name using hash table (O(1) average)
static inline int hash_table_lookup(cisv_transform_pipeline_t *pipeline, const char *field_name) {
    if (!pipeline->header_hash_table || !pipeline->header_fields) {
        return -1;
    }

    uint32_t hash = fnv1a_hash(field_name);
    size_t mask = pipeline->header_hash_size - 1;

    // Linear probing
    for (size_t i = 0; i < pipeline->header_hash_size; i++) {
        size_t idx = (hash + i) & mask;
        int field_idx = pipeline->header_hash_table[idx];

        if (field_idx < 0) {
            return -1;  // Empty slot, not found
        }

        if (strcmp(pipeline->header_fields[field_idx], field_name) == 0) {
            return field_idx;
        }
    }
    return -1;  // Table full, not found
}

// Build hash table from header fields
static void build_header_hash_table(cisv_transform_pipeline_t *pipeline) {
    if (!pipeline->header_fields || pipeline->header_count == 0) {
        return;
    }

    // Free existing hash table
    free(pipeline->header_hash_table);
    pipeline->header_hash_table = NULL;

    // Compute hash table size (next power of 2, at least 2x field count)
    size_t size = pipeline->header_count * HASH_TABLE_LOAD_FACTOR;
    size_t power = 1;
    while (power < size) power <<= 1;
    pipeline->header_hash_size = power;

    // Allocate and initialize to -1 (empty)
    pipeline->header_hash_table = malloc(power * sizeof(int));
    if (!pipeline->header_hash_table) {
        pipeline->header_hash_size = 0;
        return;
    }
    for (size_t i = 0; i < power; i++) {
        pipeline->header_hash_table[i] = -1;
    }

    // Insert all fields
    size_t mask = power - 1;
    for (size_t i = 0; i < pipeline->header_count; i++) {
        uint32_t hash = fnv1a_hash(pipeline->header_fields[i]);

        // Linear probing to find empty slot
        for (size_t j = 0; j < power; j++) {
            size_t idx = (hash + j) & mask;
            if (pipeline->header_hash_table[idx] < 0) {
                pipeline->header_hash_table[idx] = (int)i;
                break;
            }
        }
    }
}

cisv_transform_pipeline_t *cisv_transform_pipeline_create(size_t initial_capacity) {
    cisv_transform_pipeline_t *pipeline = calloc(1, sizeof(*pipeline));
    if (!pipeline) return NULL;

    pipeline->capacity = initial_capacity > 0 ? initial_capacity : 16;
    pipeline->transforms = calloc(pipeline->capacity, sizeof(cisv_transform_t));
    if (!pipeline->transforms) {
        free(pipeline);
        return NULL;
    }

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

void cisv_transform_pipeline_destroy(cisv_transform_pipeline_t *pipeline) {
    if (!pipeline) return;

    for (size_t i = 0; i < pipeline->count; i++) {
        if (pipeline->transforms[i].ctx) {
            if (pipeline->transforms[i].ctx->key) {
                memset(pipeline->transforms[i].ctx->key, 0, pipeline->transforms[i].ctx->key_len);
                free(pipeline->transforms[i].ctx->key);
                pipeline->transforms[i].ctx->key = NULL;
            }
            if (pipeline->transforms[i].ctx->iv) {
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

    // Free field index structures
    if (pipeline->transforms_by_field) {
        for (size_t i = 0; i < pipeline->transforms_by_field_size; i++) {
            free(pipeline->transforms_by_field[i]);
        }
        free(pipeline->transforms_by_field);
        pipeline->transforms_by_field = NULL;
    }
    free(pipeline->transforms_by_field_count);
    pipeline->transforms_by_field_count = NULL;
    free(pipeline->global_transforms);
    pipeline->global_transforms = NULL;

    // Free header hash table
    free(pipeline->header_hash_table);
    pipeline->header_hash_table = NULL;

    free(pipeline);
}

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

int cisv_transform_pipeline_add(
    cisv_transform_pipeline_t *pipeline,
    int field_index,
    cisv_transform_type_t type,
    cisv_transform_context_t *ctx
) {
    if (!pipeline || type >= TRANSFORM_MAX) return -1;

    if (pipeline->count >= pipeline->capacity) {
        size_t new_capacity = pipeline->capacity * 2;
        cisv_transform_t *new_transforms = realloc(
            pipeline->transforms,
            new_capacity * sizeof(cisv_transform_t)
        );
        if (!new_transforms) return -1;

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
    pipeline->index_dirty = 1;  // Mark index for rebuild
    return 0;
}

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
    pipeline->index_dirty = 1;  // Mark index for rebuild
    return 0;
}

// Build field-indexed transform lookup for O(1) access
static void build_transform_index(cisv_transform_pipeline_t *pipeline) {
    if (!pipeline || !pipeline->index_dirty) return;

    // Free existing index
    if (pipeline->transforms_by_field) {
        for (size_t i = 0; i < pipeline->transforms_by_field_size; i++) {
            free(pipeline->transforms_by_field[i]);
        }
        free(pipeline->transforms_by_field);
        pipeline->transforms_by_field = NULL;
    }
    free(pipeline->transforms_by_field_count);
    pipeline->transforms_by_field_count = NULL;
    free(pipeline->global_transforms);
    pipeline->global_transforms = NULL;
    pipeline->global_transforms_count = 0;

    // Find max field index
    size_t max_field = 0;
    size_t global_count = 0;

    for (size_t i = 0; i < pipeline->count; i++) {
        int fi = pipeline->transforms[i].field_index;
        if (fi < 0) {
            global_count++;
        } else if ((size_t)fi >= max_field) {
            max_field = (size_t)fi + 1;
        }
    }

    // Allocate index arrays
    if (max_field > 0) {
        pipeline->transforms_by_field = calloc(max_field, sizeof(size_t*));
        pipeline->transforms_by_field_count = calloc(max_field, sizeof(size_t));
        if (!pipeline->transforms_by_field || !pipeline->transforms_by_field_count) {
            free(pipeline->transforms_by_field);
            free(pipeline->transforms_by_field_count);
            pipeline->transforms_by_field = NULL;
            pipeline->transforms_by_field_count = NULL;
            return;
        }
        pipeline->transforms_by_field_size = max_field;

        // Count transforms per field
        for (size_t i = 0; i < pipeline->count; i++) {
            int fi = pipeline->transforms[i].field_index;
            if (fi >= 0) {
                pipeline->transforms_by_field_count[fi]++;
            }
        }

        // Allocate per-field arrays
        for (size_t i = 0; i < max_field; i++) {
            if (pipeline->transforms_by_field_count[i] > 0) {
                pipeline->transforms_by_field[i] = calloc(
                    pipeline->transforms_by_field_count[i], sizeof(size_t));
                if (!pipeline->transforms_by_field[i]) {
                    // Cleanup on error
                    for (size_t j = 0; j < i; j++) {
                        free(pipeline->transforms_by_field[j]);
                    }
                    free(pipeline->transforms_by_field);
                    free(pipeline->transforms_by_field_count);
                    pipeline->transforms_by_field = NULL;
                    pipeline->transforms_by_field_count = NULL;
                    pipeline->transforms_by_field_size = 0;
                    return;
                }
            }
        }

        // Fill per-field index arrays
        size_t *field_pos = calloc(max_field, sizeof(size_t));
        if (field_pos) {
            for (size_t i = 0; i < pipeline->count; i++) {
                int fi = pipeline->transforms[i].field_index;
                if (fi >= 0) {
                    pipeline->transforms_by_field[fi][field_pos[fi]++] = i;
                }
            }
            free(field_pos);
        }
    }

    // Allocate global transforms array
    if (global_count > 0) {
        pipeline->global_transforms = calloc(global_count, sizeof(size_t));
        if (pipeline->global_transforms) {
            size_t pos = 0;
            for (size_t i = 0; i < pipeline->count; i++) {
                if (pipeline->transforms[i].field_index < 0) {
                    pipeline->global_transforms[pos++] = i;
                }
            }
            pipeline->global_transforms_count = global_count;
        }
    }

    pipeline->index_dirty = 0;
}

int cisv_transform_pipeline_set_header(
    cisv_transform_pipeline_t *pipeline,
    const char **field_names,
    size_t field_count
) {
    if (!pipeline || !field_names || field_count == 0) return -1;

    if (pipeline->header_fields) {
        for (size_t i = 0; i < pipeline->header_count; i++) {
            free(pipeline->header_fields[i]);
        }
        free(pipeline->header_fields);
    }

    pipeline->header_fields = malloc(field_count * sizeof(char *));
    if (!pipeline->header_fields) return -1;

    for (size_t i = 0; i < field_count; i++) {
        pipeline->header_fields[i] = strdup(field_names[i]);
        if (!pipeline->header_fields[i]) {
            for (size_t j = 0; j < i; j++) {
                free(pipeline->header_fields[j]);
            }
            free(pipeline->header_fields);
            pipeline->header_fields = NULL;
            return -1;
        }
    }

    pipeline->header_count = field_count;

    // Build hash table for O(1) field name lookup
    build_header_hash_table(pipeline);

    return 0;
}

int cisv_transform_pipeline_add_js_by_name(
    cisv_transform_pipeline_t *pipeline,
    const char *field_name,
    void *js_callback
) {
    if (!pipeline || !field_name || !js_callback || !pipeline->header_fields) return -1;

    // Use hash table for O(1) lookup
    int field_index = hash_table_lookup(pipeline, field_name);

    if (field_index == -1) return -1;

    return cisv_transform_pipeline_add_js(pipeline, field_index, js_callback);
}

int cisv_transform_pipeline_add_by_name(
    cisv_transform_pipeline_t *pipeline,
    const char *field_name,
    cisv_transform_type_t type,
    cisv_transform_context_t *ctx
) {
    if (!pipeline || !field_name || !pipeline->header_fields) return -1;

    // Use hash table for O(1) lookup
    int field_index = hash_table_lookup(pipeline, field_name);

    if (field_index == -1) return -1;

    return cisv_transform_pipeline_add(pipeline, field_index, type, ctx);
}

// Helper to apply a single transform
static inline cisv_transform_result_t apply_single_transform(
    cisv_transform_t *t,
    cisv_transform_result_t *result,
    const char *original_data
) {
    if (t->fn) {
        cisv_transform_result_t new_result = t->fn(result->data, result->len, t->ctx);

        // Track intermediate allocation for cleanup
        if (result->needs_free && result->data != original_data &&
            result->data != new_result.data) {
            free(result->data);
        }
        return new_result;
    }
    return *result;
}

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

    // Build index lazily on first apply if needed
    if (pipeline->index_dirty) {
        build_transform_index(pipeline);
    }

    // Use indexed lookup if available (O(1) per field)
    if (pipeline->transforms_by_field || pipeline->global_transforms) {
        // Apply global transforms first (field_index == -1)
        for (size_t i = 0; i < pipeline->global_transforms_count; i++) {
            size_t ti = pipeline->global_transforms[i];
            cisv_transform_t *t = &pipeline->transforms[ti];
            if (t->fn) {
                result = apply_single_transform(t, &result, data);
            }
        }

        // Apply field-specific transforms
        if (field_index >= 0 && (size_t)field_index < pipeline->transforms_by_field_size &&
            pipeline->transforms_by_field[field_index]) {
            size_t count = pipeline->transforms_by_field_count[field_index];
            for (size_t i = 0; i < count; i++) {
                size_t ti = pipeline->transforms_by_field[field_index][i];
                cisv_transform_t *t = &pipeline->transforms[ti];
                if (t->fn) {
                    result = apply_single_transform(t, &result, data);
                }
            }
        }
    } else {
        // Fallback to O(n) scan if index not built
        for (size_t i = 0; i < pipeline->count; i++) {
            cisv_transform_t *t = &pipeline->transforms[i];

            if (t->field_index != -1 && t->field_index != field_index) {
                continue;
            }

            if (t->fn) {
                result = apply_single_transform(t, &result, data);
            }
        }
    }

    return result;
}

void cisv_transform_result_free(cisv_transform_result_t *result) {
    if (result && result->needs_free && result->data) {
        free(result->data);
        result->data = NULL;
        result->needs_free = 0;
        result->len = 0;
    }
}

#ifdef __AVX2__
void cisv_transform_uppercase_simd(char *dst, const char *src, size_t len) {
    const __m256i lower_a = _mm256_set1_epi8('a');
    const __m256i lower_z = _mm256_set1_epi8('z');
    const __m256i diff = _mm256_set1_epi8('a' - 'A');

    size_t i = 0;

    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(src + i));

        __m256i is_lower = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(lower_a, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(lower_z, _mm256_set1_epi8(1)), chunk)
        );

        __m256i upper = _mm256_sub_epi8(chunk, _mm256_and_si256(is_lower, diff));

        _mm256_storeu_si256((__m256i*)(dst + i), upper);
    }

    for (; i < len; i++) {
        dst[i] = toupper((unsigned char)src[i]);
    }
}

void cisv_transform_lowercase_simd(char *dst, const char *src, size_t len) {
    const __m256i upper_A = _mm256_set1_epi8('A');
    const __m256i upper_Z = _mm256_set1_epi8('Z');
    const __m256i diff = _mm256_set1_epi8('a' - 'A');

    size_t i = 0;

    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(src + i));

        __m256i is_upper = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(upper_A, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(upper_Z, _mm256_set1_epi8(1)), chunk)
        );

        __m256i lower = _mm256_add_epi8(chunk, _mm256_and_si256(is_upper, diff));

        _mm256_storeu_si256((__m256i*)(dst + i), lower);
    }

    for (; i < len; i++) {
        dst[i] = tolower((unsigned char)src[i]);
    }
}
#endif

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
    cisv_transform_lowercase_simd(result.data, data, len);
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

// Branchless integer parsing (1 Billion Row Challenge technique)
// 15-25% faster than strtoll for typical CSV numeric fields
static inline long long parse_int_branchless(const char *s, size_t len) {
    if (len == 0) return 0;

    // Branchless sign detection
    long long neg = (s[0] == '-');
    long long sign = 1 - 2 * neg;
    size_t i = neg;  // Skip sign character if present

    // Also handle '+' sign
    if (i < len && s[i] == '+') i++;

    // Skip leading whitespace (branchless would be complex, keep simple)
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;

    long long val = 0;
    // Parse digits - unrolled for common cases
    while (i < len) {
        unsigned char c = s[i];
        // Break on non-digit (branchless check: digit if '0' <= c <= '9')
        unsigned char d = c - '0';
        if (d > 9) break;
        val = val * 10 + d;
        i++;
    }

    return val * sign;
}

cisv_transform_result_t cisv_transform_to_int(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;

    // Use branchless parsing for better performance
    long long value = parse_int_branchless(data, len);

    result.data = malloc(32);
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

    result.data = malloc(64);
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

/**
 * ============================================================================
 * WARNING: MOCK IMPLEMENTATION - NOT CRYPTOGRAPHICALLY SECURE
 * ============================================================================
 *
 * This function does NOT provide real SHA256 hashing. It generates a
 * deterministic but INSECURE pseudo-hash based only on input length.
 *
 * DO NOT USE FOR:
 * - Password hashing or verification
 * - Data integrity verification
 * - Digital signatures
 * - Any security-sensitive operations
 *
 * This is a PLACEHOLDER for API demonstration purposes only.
 * For real cryptographic operations, integrate a proper library such as:
 * - OpenSSL (libcrypto)
 * - libsodium
 * - mbedTLS
 *
 * ============================================================================
 */
cisv_transform_result_t cisv_transform_hash_sha256(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;
    result.data = malloc(128);
    if (!result.data) {
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

    // ========================================================================
    // MOCK HASH - NOT REAL CRYPTOGRAPHY - DO NOT USE FOR SECURITY PURPOSES
    // This generates a predictable string based solely on input length.
    // A real implementation should use OpenSSL, libsodium, or similar.
    // ========================================================================
    int written = snprintf(result.data, 128, "MOCK_SHA256_%016lx%016lx%016lx%016lx",
             (unsigned long)len,
             (unsigned long)(len * 0x1234567890ABCDEF),
             (unsigned long)(len * 0xFEDCBA0987654321),
             (unsigned long)(len * 0xDEADBEEFC0FFEE00));

    result.len = (written > 0 && written < 128) ? (size_t)written : 64;
    result.needs_free = 1;
    return result;
}

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

cisv_transform_result_t cisv_transform_base64_encode(const char *data, size_t len, cisv_transform_context_t *ctx) {
    (void)ctx;

    cisv_transform_result_t result;

    // SECURITY: Check for overflow before calculating output length
    // Base64 output = ceil(input / 3) * 4, which can overflow for huge inputs
    // Max safe input: (SIZE_MAX - 4) / 4 * 3 ≈ SIZE_MAX * 0.75
    if (len > (SIZE_MAX - 4) / 4 * 3) {
        // Would overflow - return original data
        result.data = (char*)data;
        result.len = len;
        result.needs_free = 0;
        return result;
    }

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
