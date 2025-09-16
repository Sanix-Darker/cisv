#ifndef CISV_PARSER_AVX2_H
#define CISV_PARSER_AVX2_H

#include <immintrin.h>
#include "cisv_parser.h"

#define CISV_AVX2_VEC_BYTES 32
typedef __m256i cisv_avx2_vec;

void cisv_parser_avx2_init(void);
void cisv_parse_simd_chunk_avx2(cisv_parser *parser, const uint8_t *buffer, size_t len);
const uint8_t* cisv_trim_start_avx2(const uint8_t *start, const uint8_t *end);
const uint8_t* cisv_trim_end_avx2(const uint8_t *start, const uint8_t *end);

#endif
