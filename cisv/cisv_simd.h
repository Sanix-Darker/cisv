#ifndef cisv_SIMD_H
#define cisv_SIMD_H

#include <stdint.h>

// Detect x86-64 architecture
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    // Check for AVX-512 support
    #if defined(__AVX512F__) && defined(__AVX512BW__)
        #define cisv_ARCH_AVX512 1
        #define cisv_HAVE_AVX512 1
        typedef __m512i cisv_vec;
        #define cisv_VEC_BYTES 64
        #define cisv_LOAD(p) _mm512_loadu_si512((const __m512i *)(p))
        #define cisv_CMP_EQ(a, b) _mm512_cmpeq_epi8_mask(a, b)
        #define cisv_MOVEMASK(m) (m) // Already a mask
        #define cisv_CTZ(m) _tzcnt_u64(m)
    // Fallback to AVX2
    #elif defined(__AVX2__)
        #define cisv_ARCH_AVX2 1
        #define cisv_HAVE_AVX2 1
        typedef __m256i cisv_vec;
        #define cisv_VEC_BYTES 32
        #define cisv_LOAD(p) _mm256_loadu_si256((const __m256i *)(p))
        #define cisv_CMP_EQ(a, b) _mm256_cmpeq_epi8(a, b)
        #define cisv_MOVEMASK(v) _mm256_movemask_epi8(v)
        #define cisv_OR_MASK(a, b) _mm256_or_si256(a, b)
        #define cisv_CTZ(m) _tzcnt_u32(m)
    #else
        // x86-64 but no AVX2, use generic fallback
        #define cisv_ARCH_GENERIC 1
    #endif
#else
    // Not x86-64, use generic fallback
    #define cisv_ARCH_GENERIC 1
#endif


// Generic, non-SIMD fallback for unsupported architectures (like ARM)
#if defined(cisv_ARCH_GENERIC)
    // This path makes the "SIMD" functions operate on a single byte at a time.
    typedef uint8_t cisv_vec;
    #define cisv_VEC_BYTES 1
    // These macros will not be used in the scalar path but are defined for completeness.
    #define cisv_LOAD(p) (*(p))
    #define cisv_CMP_EQ(a, b) (a == b)
    #define cisv_MOVEMASK(v) (v)
    #define cisv_OR_MASK(a, b) (a | b)
    #define cisv_CTZ(m) (0)
#endif

#endif // cisv_SIMD_H
