"""
CISV - High-performance CSV parser with SIMD optimizations

This module provides Python bindings to the CISV C library using ctypes.
"""

from .parser import (
    CisvParser,
    parse_file,
    parse_string,
    count_rows,
    CisvError,
    CisvValidationError,
    CisvParseError,
    MAX_FILE_SIZE,
)

__version__ = '0.1.0'
__all__ = [
    'CisvParser',
    'parse_file',
    'parse_string',
    'count_rows',
    'CisvError',
    'CisvValidationError',
    'CisvParseError',
    'MAX_FILE_SIZE',
]
