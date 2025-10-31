#ifndef CISV_SIMD_H
#define CISV_SIMD_H

#include <stdint.h>

// AVX512 support
#ifdef __AVX512F__
    #include <immintrin.h>
    #define cisv_HAVE_AVX512
    #define cisv_VEC_BYTES 64
    typedef __m512i cisv_vec;
    #define cisv_LOAD(p) _mm512_loadu_si512((const __m512i*)(p))
    #define cisv_CMP_EQ(a, b) _mm512_cmpeq_epi8_mask(a, b)
    #define cisv_OR_MASK(a, b) ((a) | (b))
    #define cisv_CTZ(x) __builtin_ctzll(x)
    #define cisv_MOVEMASK(x) (x)  // AVX512 already returns mask

// AVX2 support
#elif defined(__AVX2__)
    #include <immintrin.h>
    #define cisv_HAVE_AVX2
    #define cisv_VEC_BYTES 32
    typedef __m256i cisv_vec;
    #define cisv_LOAD(p) _mm256_loadu_si256((const __m256i*)(p))
    #define cisv_CMP_EQ(a, b) _mm256_cmpeq_epi8(a, b)
    #define cisv_OR_MASK(a, b) _mm256_or_si256(a, b)
    #define cisv_MOVEMASK(x) _mm256_movemask_epi8(x)
    #define cisv_CTZ(x) __builtin_ctz(x)

// SSE2 support (baseline for x86-64)
#elif defined(__SSE2__)
    #include <emmintrin.h>
    #define cisv_HAVE_SSE2
    #define cisv_VEC_BYTES 16
    typedef __m128i cisv_vec;
    #define cisv_LOAD(p) _mm_loadu_si128((const __m128i*)(p))
    #define cisv_CMP_EQ(a, b) _mm_cmpeq_epi8(a, b)
    #define cisv_OR_MASK(a, b) _mm_or_si128(a, b)
    #define cisv_MOVEMASK(x) _mm_movemask_epi8(x)
    #define cisv_CTZ(x) __builtin_ctz(x)

// ARM NEON support
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define HAS_NEON
    // NEON doesn't use the same macro pattern, handled separately
#endif

// Compile-time SIMD detection and configuration
#if defined(cisv_HAVE_AVX512) || defined(cisv_HAVE_AVX2) || defined(cisv_HAVE_SSE2)
    #define cisv_HAVE_SIMD
#endif

#endif // CISV_SIMD_H
