#ifndef CISV_PARSER_SCALAR_H
#define CISV_PARSER_SCALAR_H

#include "cisv_parser.h"

void cisv_parse_simd_chunk_scalar(cisv_parser *parser, const uint8_t *buffer, size_t len);
const uint8_t* cisv_trim_start_scalar(const uint8_t *start, const uint8_t *end);
const uint8_t* cisv_trim_end_scalar(const uint8_t *start, const uint8_t *end);

#endif
