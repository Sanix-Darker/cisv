# CISV Python Bindings (nanobind)

High-performance Python bindings for the CISV CSV parser using [nanobind](https://github.com/wjakob/nanobind).

## Performance

These bindings are 10-100x faster than the ctypes-based bindings because they:

1. **Use the batch API**: All data is parsed in C and returned at once, eliminating millions of per-field callbacks
2. **Use nanobind**: Much lower overhead than ctypes or pybind11
3. **Release the GIL**: Parallel parsing runs without holding the Python GIL

| File Size | ctypes | nanobind | Speedup |
|-----------|--------|----------|---------|
| 142MB (1M rows × 10 cols) | ~20s | <0.8s | 25x+ |

## Installation

### From PyPI (recommended)

```bash
pip install cisv
```

### From source

```bash
cd bindings/python-nanobind
pip install .
```

### Development install

```bash
cd bindings/python-nanobind
pip install -e .
```

## Usage

```python
import cisv

# Parse a file
rows = cisv.parse_file('data.csv')

# Parse with options
rows = cisv.parse_file(
    'data.csv',
    delimiter=';',
    quote="'",
    trim=True,
    skip_empty_lines=True
)

# Parse large files in parallel (faster on multi-core systems)
rows = cisv.parse_file('large.csv', parallel=True)

# Parse a string
rows = cisv.parse_string("a,b,c\n1,2,3")

# Count rows quickly (SIMD-accelerated)
count = cisv.count_rows('data.csv')

# Row-by-row iteration (memory efficient, supports early exit)
with cisv.CisvIterator('large.csv') as reader:
    for row in reader:
        print(row)  # List[str]
        if row[0] == 'stop':
            break  # Early exit - no wasted work

# Or use the convenience function
for row in cisv.open_iterator('data.csv', delimiter=',', trim=True):
    process(row)
```

## API Reference

### `parse_file(path, delimiter=',', quote='"', *, trim=False, skip_empty_lines=False, parallel=False, num_threads=0)`

Parse a CSV file and return all rows.

**Parameters:**
- `path`: Path to the CSV file
- `delimiter`: Field delimiter character (default: ',')
- `quote`: Quote character (default: '"')
- `trim`: Whether to trim whitespace from fields
- `skip_empty_lines`: Whether to skip empty lines
- `parallel`: Use multi-threaded parsing (faster for large files)
- `num_threads`: Number of threads for parallel parsing (0 = auto-detect)

**Returns:** List of rows, where each row is a list of field values.

### `parse_string(content, delimiter=',', quote='"', *, trim=False, skip_empty_lines=False)`

Parse a CSV string and return all rows.

**Parameters:**
- `content`: CSV content as a string
- `delimiter`: Field delimiter character (default: ',')
- `quote`: Quote character (default: '"')
- `trim`: Whether to trim whitespace from fields
- `skip_empty_lines`: Whether to skip empty lines

**Returns:** List of rows, where each row is a list of field values.

### `count_rows(path)`

Count the number of rows in a CSV file without full parsing.

This is very fast as it only scans for newlines using SIMD instructions.

**Parameters:**
- `path`: Path to the CSV file

**Returns:** Number of rows in the file.

### `CisvIterator(path, delimiter=',', quote='"', *, trim=False, skip_empty_lines=False)`

Row-by-row iterator for streaming CSV parsing with minimal memory footprint.

Provides fgetcsv-style iteration that supports early exit - breaking out of iteration stops parsing immediately with no wasted work.

**Parameters:**
- `path`: Path to the CSV file
- `delimiter`: Field delimiter character (default: ',')
- `quote`: Quote character (default: '"')
- `trim`: Whether to trim whitespace from fields
- `skip_empty_lines`: Whether to skip empty lines

**Methods:**
- `next()`: Get the next row as `List[str]`, or `None` if at end of file
- `close()`: Close the iterator and release resources
- `closed`: Property indicating whether the iterator has been closed

**Protocols:**
- **Iterator protocol**: Use in `for` loops with `for row in iterator`
- **Context manager**: Use with `with` statement for automatic cleanup

**Example:**
```python
# Context manager (recommended)
with cisv.CisvIterator('data.csv') as reader:
    for row in reader:
        if row[0] == 'target':
            print(f"Found: {row}")
            break  # Early exit

# Manual iteration
reader = cisv.CisvIterator('data.csv')
try:
    while True:
        row = reader.next()
        if row is None:
            break
        process(row)
finally:
    reader.close()
```

### `open_iterator(path, delimiter=',', quote='"', *, trim=False, skip_empty_lines=False)`

Convenience function that returns a `CisvIterator`. Same parameters as `CisvIterator`.

**Example:**
```python
for row in cisv.open_iterator('data.csv'):
    print(row)
```

## Running Tests

```bash
cd bindings/python-nanobind
pip install -e ".[test]"
pytest
```

## Benchmarking

```bash
pip install -e ".[benchmark]"
python -c "
import cisv
import time

# Create test file
with open('/tmp/test.csv', 'w') as f:
    f.write('col1,col2,col3\n')
    for i in range(100000):
        f.write(f'value{i}_1,value{i}_2,value{i}_3\n')

# Benchmark
start = time.time()
rows = cisv.parse_file('/tmp/test.csv')
print(f'Parsed {len(rows)} rows in {time.time()-start:.3f}s')
"
```
