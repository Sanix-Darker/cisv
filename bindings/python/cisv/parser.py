"""
CISV Parser - Python bindings using ctypes
"""

import ctypes
import os
from pathlib import Path
from typing import List, Optional, Callable, Any

# Find the shared library
def _find_library():
    """Find the cisv shared library."""
    base_dir = Path(__file__).parent.parent.parent.parent

    # Possible library locations
    locations = [
        base_dir / 'core' / 'build' / 'libcisv.so',
        base_dir / 'core' / 'build' / 'libcisv.dylib',
        '/usr/local/lib/libcisv.so',
        '/usr/lib/libcisv.so',
    ]

    for loc in locations:
        if loc.exists():
            return str(loc)

    # Try system library path
    try:
        return ctypes.util.find_library('cisv')
    except:
        pass

    raise RuntimeError(
        "Could not find libcisv. Please build the core library first:\n"
        "  cd core && make"
    )

# Load the library
_lib = None

def _get_lib():
    global _lib
    if _lib is None:
        _lib = ctypes.CDLL(_find_library())
        _setup_bindings(_lib)
    return _lib

# Callback types
FieldCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t)
RowCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
ErrorCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p)

# Config structure - must match cisv_config in parser.h exactly
class CisvConfig(ctypes.Structure):
    _fields_ = [
        ('delimiter', ctypes.c_char),
        ('quote', ctypes.c_char),
        ('escape', ctypes.c_char),
        ('skip_empty_lines', ctypes.c_bool),
        ('comment', ctypes.c_char),
        ('trim', ctypes.c_bool),
        ('relaxed', ctypes.c_bool),
        ('max_row_size', ctypes.c_size_t),
        ('from_line', ctypes.c_int),
        ('to_line', ctypes.c_int),
        ('skip_lines_with_error', ctypes.c_bool),
        ('field_cb', FieldCallback),
        ('row_cb', RowCallback),
        ('error_cb', ErrorCallback),
        ('user', ctypes.c_void_p),
    ]

def _setup_bindings(lib):
    """Setup ctypes bindings for the library."""
    # cisv_config_init
    lib.cisv_config_init.argtypes = [ctypes.POINTER(CisvConfig)]
    lib.cisv_config_init.restype = None

    # cisv_parser_create_with_config
    lib.cisv_parser_create_with_config.argtypes = [ctypes.POINTER(CisvConfig)]
    lib.cisv_parser_create_with_config.restype = ctypes.c_void_p

    # cisv_parser_destroy
    lib.cisv_parser_destroy.argtypes = [ctypes.c_void_p]
    lib.cisv_parser_destroy.restype = None

    # cisv_parser_parse_file
    lib.cisv_parser_parse_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.cisv_parser_parse_file.restype = ctypes.c_int

    # cisv_parser_write
    lib.cisv_parser_write.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
    lib.cisv_parser_write.restype = None

    # cisv_parser_end
    lib.cisv_parser_end.argtypes = [ctypes.c_void_p]
    lib.cisv_parser_end.restype = None

    # cisv_parser_count_rows
    lib.cisv_parser_count_rows.argtypes = [ctypes.c_char_p]
    lib.cisv_parser_count_rows.restype = ctypes.c_size_t


class CisvParser:
    """High-performance CSV parser with SIMD optimizations."""

    def __init__(
        self,
        delimiter: str = ',',
        quote: str = '"',
        escape: Optional[str] = None,
        comment: Optional[str] = None,
        trim: bool = False,
        skip_empty_lines: bool = False,
    ):
        self._lib = _get_lib()
        self._rows: List[List[str]] = []
        self._current_row: List[str] = []
        self._parser = None

        # Store config
        self._delimiter = delimiter
        self._quote = quote
        self._escape = escape
        self._comment = comment
        self._trim = trim
        self._skip_empty_lines = skip_empty_lines

        # Create callbacks that store references to prevent garbage collection
        self._field_cb = FieldCallback(self._on_field)
        self._row_cb = RowCallback(self._on_row)
        self._error_cb = ErrorCallback(self._on_error)

    def _on_field(self, user: ctypes.c_void_p, data: ctypes.c_char_p, length: int):
        """Called for each field."""
        # Use ctypes.string_at to safely copy data before pointer is invalidated
        # The data pointer is only valid during this callback
        field_bytes = ctypes.string_at(data, length)
        field = field_bytes.decode('utf-8', errors='replace')
        self._current_row.append(field)

    def _on_row(self, user: ctypes.c_void_p):
        """Called at end of each row."""
        self._rows.append(self._current_row)
        self._current_row = []

    def _on_error(self, user: ctypes.c_void_p, line: int, msg: ctypes.c_char_p):
        """Called on parse error."""
        pass  # Silently ignore errors for now

    def _create_parser(self) -> ctypes.c_void_p:
        """Create a new parser instance."""
        config = CisvConfig()
        self._lib.cisv_config_init(ctypes.byref(config))

        # c_char expects bytes of length 1, not a slice
        config.delimiter = self._delimiter.encode('utf-8')[0:1]
        config.quote = self._quote.encode('utf-8')[0:1]
        if self._escape:
            config.escape = self._escape.encode('utf-8')[0:1]
        if self._comment:
            config.comment = self._comment.encode('utf-8')[0:1]
        config.trim = self._trim
        config.skip_empty_lines = self._skip_empty_lines

        config.field_cb = self._field_cb
        config.row_cb = self._row_cb
        config.error_cb = self._error_cb

        return self._lib.cisv_parser_create_with_config(ctypes.byref(config))

    def parse_file(self, path: str) -> List[List[str]]:
        """Parse a CSV file and return all rows."""
        self._rows = []
        self._current_row = []

        parser = self._create_parser()
        if not parser:
            raise RuntimeError("Failed to create parser")

        try:
            result = self._lib.cisv_parser_parse_file(parser, path.encode('utf-8'))
            if result < 0:
                raise RuntimeError(f"Parse error: {result}")
        finally:
            self._lib.cisv_parser_destroy(parser)

        return self._rows

    def parse_string(self, content: str) -> List[List[str]]:
        """Parse a CSV string and return all rows."""
        self._rows = []
        self._current_row = []

        parser = self._create_parser()
        if not parser:
            raise RuntimeError("Failed to create parser")

        try:
            data = content.encode('utf-8')
            self._lib.cisv_parser_write(parser, data, len(data))
            self._lib.cisv_parser_end(parser)
        finally:
            self._lib.cisv_parser_destroy(parser)

        return self._rows


def parse_file(
    path: str,
    delimiter: str = ',',
    quote: str = '"',
    **kwargs
) -> List[List[str]]:
    """Parse a CSV file and return all rows."""
    parser = CisvParser(delimiter=delimiter, quote=quote, **kwargs)
    return parser.parse_file(path)


def parse_string(
    content: str,
    delimiter: str = ',',
    quote: str = '"',
    **kwargs
) -> List[List[str]]:
    """Parse a CSV string and return all rows."""
    parser = CisvParser(delimiter=delimiter, quote=quote, **kwargs)
    return parser.parse_string(content)


def count_rows(path: str) -> int:
    """Count the number of rows in a CSV file without full parsing."""
    lib = _get_lib()
    return lib.cisv_parser_count_rows(path.encode('utf-8'))
