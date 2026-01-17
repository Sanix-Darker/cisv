"""
CISV - High-performance CSV parser with SIMD optimizations

This module provides Python bindings to the CISV C library using nanobind,
offering 10-100x better performance than ctypes-based bindings.
"""

from typing import List, Optional

from ._core import (
    parse_file as _parse_file,
    parse_string as _parse_string,
    parse_file_parallel as _parse_file_parallel,
    count_rows,
)

__version__ = '0.2.0'
__all__ = [
    'parse_file',
    'parse_string',
    'count_rows',
    'CisvError',
]


class CisvError(Exception):
    """Base exception for CISV errors."""
    pass


def parse_file(
    path: str,
    delimiter: str = ',',
    quote: str = '"',
    *,
    trim: bool = False,
    skip_empty_lines: bool = False,
    parallel: bool = False,
    num_threads: int = 0,
) -> List[List[str]]:
    """
    Parse a CSV file and return all rows.

    This function uses SIMD-optimized C code for maximum performance.
    For large files (>10MB), consider using parallel=True for even
    better performance on multi-core systems.

    Args:
        path: Path to the CSV file
        delimiter: Field delimiter character (default: ',')
        quote: Quote character (default: '"')
        trim: Whether to trim whitespace from fields
        skip_empty_lines: Whether to skip empty lines
        parallel: Use multi-threaded parsing (faster for large files)
        num_threads: Number of threads for parallel parsing (0 = auto-detect)

    Returns:
        List of rows, where each row is a list of field values.

    Raises:
        CisvError: If parsing fails
        ValueError: If invalid arguments are provided

    Example:
        >>> import cisv
        >>> rows = cisv.parse_file('data.csv')
        >>> for row in rows:
        ...     print(row)
        ['header1', 'header2', 'header3']
        ['value1', 'value2', 'value3']
    """
    try:
        if parallel:
            return _parse_file_parallel(
                path, num_threads, delimiter, quote, trim, skip_empty_lines
            )
        return _parse_file(path, delimiter, quote, trim, skip_empty_lines)
    except RuntimeError as e:
        raise CisvError(str(e)) from e
    except Exception as e:
        raise CisvError(f"Failed to parse file: {e}") from e


def parse_string(
    content: str,
    delimiter: str = ',',
    quote: str = '"',
    *,
    trim: bool = False,
    skip_empty_lines: bool = False,
) -> List[List[str]]:
    """
    Parse a CSV string and return all rows.

    Args:
        content: CSV content as a string
        delimiter: Field delimiter character (default: ',')
        quote: Quote character (default: '"')
        trim: Whether to trim whitespace from fields
        skip_empty_lines: Whether to skip empty lines

    Returns:
        List of rows, where each row is a list of field values.

    Raises:
        CisvError: If parsing fails
        ValueError: If invalid arguments are provided

    Example:
        >>> import cisv
        >>> csv_data = "a,b,c\\n1,2,3"
        >>> rows = cisv.parse_string(csv_data)
        >>> print(rows)
        [['a', 'b', 'c'], ['1', '2', '3']]
    """
    try:
        return _parse_string(content, delimiter, quote, trim, skip_empty_lines)
    except RuntimeError as e:
        raise CisvError(str(e)) from e
    except Exception as e:
        raise CisvError(f"Failed to parse string: {e}") from e
