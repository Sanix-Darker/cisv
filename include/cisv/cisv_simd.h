#ifndef CISV_SIMD_H
#define CISV_SIMD_H

// Detect SIMD support
#if defined(__AVX512F__)
    #define CISV_HAVE_AVX512 1
#elif defined(__AVX2__)
    #define CISV_HAVE_AVX2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define CISV_HAVE_NEON 1
#endif

#endif // CISV_SIMD_H
