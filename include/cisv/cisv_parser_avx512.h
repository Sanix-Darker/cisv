#ifndef CISV_PARSER_AVX512_H
#define CISV_PARSER_AVX512_H

#include <immintrin.h>
#include "cisv_parser.h"

#define CISV_AVX512_VEC_BYTES 64
typedef __m512i cisv_avx512_vec;

void cisv_parser_avx512_init(void);
void cisv_parse_simd_chunk_avx512(cisv_parser *parser, const uint8_t *buffer, size_t len);
const uint8_t* cisv_trim_start_avx512(const uint8_t *start, const uint8_t *end);
const uint8_t* cisv_trim_end_avx512(const uint8_t *start, const uint8_t *end);

#endif
