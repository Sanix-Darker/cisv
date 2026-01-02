# CISV Python Binding

High-performance CSV parser with SIMD optimizations for Python.

## Requirements

- Python 3.7+
- CISV core library (`libcisv.so`)

## Installation

### Build Core Library First

```bash
cd ../../core
make
```

### Install Python Package

```bash
pip install -e .
```

Or using the Makefile:

```bash
make build
```

## Quick Start

```python
from cisv import CisvParser, parse_file, parse_string, count_rows

# Simple file parsing
rows = parse_file('data.csv')
for row in rows:
    print(row)

# Parse with custom options
parser = CisvParser(
    delimiter=';',
    quote="'",
    trim=True
)
rows = parser.parse_file('data.csv')

# Parse from string
csv_data = """name,age,email
John,30,john@example.com
Jane,25,jane@example.com"""

rows = parse_string(csv_data)

# Fast row counting (without full parsing)
total = count_rows('large.csv')
print(f"Total rows: {total}")
```

## API Reference

### CisvParser Class

```python
class CisvParser:
    def __init__(
        self,
        delimiter: str = ',',
        quote: str = '"',
        escape: Optional[str] = None,
        comment: Optional[str] = None,
        trim: bool = False,
        skip_empty_lines: bool = False,
    ):
        """
        Create a new CSV parser.

        Args:
            delimiter: Field separator character (default: ',')
            quote: Quote character for fields (default: '"')
            escape: Escape character (default: None for RFC4180 "" style)
            comment: Comment line prefix (default: None)
            trim: Strip whitespace from fields (default: False)
            skip_empty_lines: Skip empty lines (default: False)
        """

    def parse_file(self, path: str) -> List[List[str]]:
        """Parse a CSV file and return all rows."""

    def parse_string(self, content: str) -> List[List[str]]:
        """Parse a CSV string and return all rows."""
```

### Convenience Functions

```python
def parse_file(
    path: str,
    delimiter: str = ',',
    quote: str = '"',
    **kwargs
) -> List[List[str]]:
    """Parse a CSV file with the given options."""

def parse_string(
    content: str,
    delimiter: str = ',',
    quote: str = '"',
    **kwargs
) -> List[List[str]]:
    """Parse a CSV string with the given options."""

def count_rows(path: str) -> int:
    """Count rows in a CSV file without full parsing."""
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `delimiter` | str | `','` | Field delimiter character |
| `quote` | str | `'"'` | Quote character |
| `escape` | str | `None` | Escape character |
| `comment` | str | `None` | Comment line prefix |
| `trim` | bool | `False` | Trim whitespace from fields |
| `skip_empty_lines` | bool | `False` | Skip empty lines |

## Examples

### TSV Parsing

```python
from cisv import CisvParser

parser = CisvParser(delimiter='\t')
rows = parser.parse_file('data.tsv')
```

### Skip Comments and Empty Lines

```python
parser = CisvParser(
    comment='#',
    skip_empty_lines=True,
    trim=True
)
rows = parser.parse_file('config.csv')
```

### Parse CSV String

```python
from cisv import parse_string

data = """
id,name,value
1,foo,100
2,bar,200
"""

rows = parse_string(data, trim=True)
# [['id', 'name', 'value'], ['1', 'foo', '100'], ['2', 'bar', '200']]
```

## Performance

CISV uses SIMD optimizations (AVX-512, AVX2, SSE2) for high-performance parsing. The Python binding uses ctypes to call directly into the native C library with minimal overhead.

Typical performance on modern hardware:
- 500MB+ CSV files parsed in under 1 second
- 10-50x faster than pure Python CSV parsers

## License

MIT
