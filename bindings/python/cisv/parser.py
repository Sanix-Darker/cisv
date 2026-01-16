"""
CISV Parser - Python bindings using ctypes
"""

import ctypes
import os
import stat
from pathlib import Path
from typing import List, Optional, Callable, Any


class CisvError(Exception):
    """Base exception for CISV errors."""
    pass


class CisvValidationError(CisvError):
    """Raised when input validation fails."""
    pass


class CisvParseError(CisvError):
    """Raised when parsing fails."""
    pass


# Maximum file size to process (default 10GB)
MAX_FILE_SIZE = 10 * 1024 * 1024 * 1024

# Find the shared library
def _find_library():
    """Find the cisv shared library."""
    pkg_dir = Path(__file__).parent

    # First, check for bundled library in package (installed via pip)
    bundled_locations = [
        pkg_dir / 'libs' / 'libcisv.so',
        pkg_dir / 'libs' / 'libcisv.dylib',
        pkg_dir / 'libcisv.so',
        pkg_dir / 'libcisv.dylib',
    ]

    for loc in bundled_locations:
        if loc.exists():
            return str(loc)

    # Fallback to development locations (when running from source)
    base_dir = pkg_dir.parent.parent.parent

    dev_locations = [
        base_dir / 'core' / 'build' / 'libcisv.so',
        base_dir / 'core' / 'build' / 'libcisv.dylib',
    ]

    for loc in dev_locations:
        if loc.exists():
            return str(loc)

    # System library paths
    system_locations = [
        Path('/usr/local/lib/libcisv.so'),
        Path('/usr/local/lib/libcisv.dylib'),
        Path('/usr/lib/libcisv.so'),
    ]

    for loc in system_locations:
        if loc.exists():
            return str(loc)

    # Try system library path via ctypes
    try:
        lib_path = ctypes.util.find_library('cisv')
        if lib_path:
            return lib_path
    except Exception:
        pass

    raise RuntimeError(
        "Could not find libcisv shared library.\n"
        "If you installed via pip, this may indicate a packaging issue.\n"
        "If running from source, build the core library first:\n"
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
        max_file_size: int = MAX_FILE_SIZE,
        raise_on_error: bool = True,
    ):
        self._lib = _get_lib()
        self._rows: List[List[str]] = []
        self._current_row: List[str] = []
        self._parser = None
        self._parse_errors: List[tuple] = []

        # SECURITY: Validate delimiter
        if not delimiter:
            raise CisvValidationError("Delimiter cannot be empty")
        if len(delimiter) > 1:
            raise CisvValidationError(
                f"Delimiter must be a single character, got '{delimiter}' "
                f"(length {len(delimiter)}). Multi-byte delimiters are not supported."
            )

        # SECURITY: Validate quote character
        if not quote:
            raise CisvValidationError("Quote character cannot be empty")
        if len(quote) > 1:
            raise CisvValidationError(
                f"Quote must be a single character, got '{quote}' "
                f"(length {len(quote)}). Multi-byte quote characters are not supported."
            )

        # SECURITY: Validate escape character
        if escape is not None and len(escape) > 1:
            raise CisvValidationError(
                f"Escape must be a single character, got '{escape}' "
                f"(length {len(escape)}). Multi-byte escape characters are not supported."
            )

        # SECURITY: Validate comment character
        if comment is not None and len(comment) > 1:
            raise CisvValidationError(
                f"Comment must be a single character, got '{comment}' "
                f"(length {len(comment)}). Multi-byte comment characters are not supported."
            )

        # SECURITY: Validate delimiter/quote are different
        if delimiter == quote:
            raise CisvValidationError(
                f"Delimiter and quote character cannot be the same ('{delimiter}')"
            )

        # Store config
        self._delimiter = delimiter
        self._quote = quote
        self._escape = escape
        self._comment = comment
        self._trim = trim
        self._skip_empty_lines = skip_empty_lines
        self._max_file_size = max_file_size
        self._raise_on_error = raise_on_error

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
        # SECURITY FIX: Don't silently ignore errors
        error_msg = msg.decode('utf-8', errors='replace') if msg else "Unknown error"
        self._parse_errors.append((line, error_msg))

        if self._raise_on_error:
            raise CisvParseError(f"Parse error at line {line}: {error_msg}")

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

    def _validate_file_path(self, path: str) -> Path:
        """
        SECURITY: Validate file path to prevent various attacks.

        Checks for:
        - Path traversal attempts
        - Symlink attacks (follows to final target, checks it's a regular file)
        - Device files (/dev/zero, /dev/random, etc.)
        - File size limits
        """
        file_path = Path(path)

        # Check if file exists
        if not file_path.exists():
            raise CisvValidationError(f"File not found: {path}")

        # Resolve symlinks and get the real path
        real_path = file_path.resolve()

        # SECURITY: Check for device files
        try:
            file_stat = real_path.stat()
            if stat.S_ISBLK(file_stat.st_mode) or stat.S_ISCHR(file_stat.st_mode):
                raise CisvValidationError(
                    f"Cannot parse device file: {path}"
                )
            if stat.S_ISFIFO(file_stat.st_mode):
                raise CisvValidationError(
                    f"Cannot parse FIFO/pipe: {path}"
                )
            if stat.S_ISSOCK(file_stat.st_mode):
                raise CisvValidationError(
                    f"Cannot parse socket: {path}"
                )
            if not stat.S_ISREG(file_stat.st_mode):
                raise CisvValidationError(
                    f"Path is not a regular file: {path}"
                )

            # SECURITY: Check file size limit
            if file_stat.st_size > self._max_file_size:
                raise CisvValidationError(
                    f"File too large: {file_stat.st_size} bytes "
                    f"(max {self._max_file_size} bytes). "
                    f"Increase max_file_size if this is intentional."
                )
        except OSError as e:
            raise CisvValidationError(f"Cannot access file {path}: {e}")

        return real_path

    def parse_file(self, path: str, validate_path: bool = True) -> List[List[str]]:
        """
        Parse a CSV file and return all rows.

        Args:
            path: Path to the CSV file
            validate_path: If True (default), validates the file path for security.
                          Set to False only if you've already validated the path.

        Returns:
            List of rows, where each row is a list of field values.

        Raises:
            CisvValidationError: If path validation fails
            CisvParseError: If parsing fails
            RuntimeError: If parser creation fails
        """
        self._rows = []
        self._current_row = []
        self._parse_errors = []

        # SECURITY: Validate file path
        if validate_path:
            real_path = self._validate_file_path(path)
            path_to_parse = str(real_path)
        else:
            path_to_parse = path

        parser = self._create_parser()
        if not parser:
            raise RuntimeError("Failed to create parser")

        try:
            result = self._lib.cisv_parser_parse_file(parser, path_to_parse.encode('utf-8'))
            if result < 0:
                raise CisvParseError(f"Parse error code: {result}")
        finally:
            self._lib.cisv_parser_destroy(parser)

        return self._rows

    @property
    def errors(self) -> List[tuple]:
        """Return list of (line_number, error_message) tuples from last parse."""
        return self._parse_errors.copy()

    def parse_string(self, content: str) -> List[List[str]]:
        """
        Parse a CSV string and return all rows.

        Args:
            content: CSV content as a string

        Returns:
            List of rows, where each row is a list of field values.

        Raises:
            CisvParseError: If parsing fails (when raise_on_error=True)
            RuntimeError: If parser creation fails
        """
        self._rows = []
        self._current_row = []
        self._parse_errors = []

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
