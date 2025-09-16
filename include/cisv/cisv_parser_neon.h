#ifndef CISV_PARSER_NEON_H
#define CISV_PARSER_NEON_H

#include <arm_neon.h>
#include "cisv_parser.h"

#define CISV_NEON_VEC_BYTES 16
typedef uint8x16_t cisv_neon_vec;

void cisv_parser_neon_init(void);
void cisv_parse_simd_chunk_neon(cisv_parser *parser, const uint8_t *buffer, size_t len);
const uint8_t* cisv_trim_start_neon(const uint8_t *start, const uint8_t *end);
const uint8_t* cisv_trim_end_neon(const uint8_t *start, const uint8_t *end);

#endif
