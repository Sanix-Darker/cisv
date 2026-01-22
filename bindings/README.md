# CISV Language Bindings

This directory contains official language bindings for the CISV high-performance CSV parser.

## Available Bindings

| Language | Directory | Status | Features |
|----------|-----------|--------|----------|
| **Node.js** | [`nodejs/`](./nodejs/) | Stable | Full API, Transforms, Iterator |
| **Python (nanobind)** | [`python-nanobind/`](./python-nanobind/) | Stable | Full API, Parallel, Iterator |
| **Python (ctypes)** | [`python/`](./python/) | Legacy | Basic API |
| **PHP** | [`php/`](./php/) | Stable | Full API, Transforms, Iterator |

## Feature Comparison

| Feature | Node.js | Python (nanobind) | PHP |
|---------|---------|-------------------|-----|
| Sync File Parsing | Yes | Yes | Yes |
| Async File Parsing | Yes | - | - |
| String Parsing | Yes | Yes | Yes |
| Streaming API | Yes | - | - |
| Row-by-Row Iterator | Yes | Yes | Yes |
| Field Transforms | Yes | - | Yes |
| Parallel Parsing | - | Yes | - |
| Count Rows (SIMD) | Yes | Yes | Yes |
| TypeScript Types | Yes | Yes (.pyi stubs) | - |

## Row-by-Row Iterator API

All bindings now support a memory-efficient row-by-row iterator API that:

- Provides **fgetcsv-style** streaming with minimal memory footprint
- Supports **early exit** - breaking out of iteration stops parsing immediately
- Returns rows one at a time without loading entire file into memory

### Python Example

```python
import cisv

with cisv.CisvIterator('large.csv') as reader:
    for row in reader:
        print(row)  # List[str]
        if row[0] == 'stop':
            break  # Early exit - no wasted work
```

### Node.js Example

```javascript
const { cisvParser } = require('cisv');

const parser = new cisvParser({ delimiter: ',' });
parser.openIterator('large.csv');

let row;
while ((row = parser.fetchRow()) !== null) {
    console.log(row);  // string[]
    if (row[0] === 'stop') break;  // Early exit
}

parser.closeIterator();
```

### PHP Example

```php
$parser = new Cisv\Parser(['delimiter' => ',']);
$iterator = $parser->openIterator('large.csv');

while (($row = $iterator->fetch()) !== null) {
    print_r($row);
    if ($row[0] === 'stop') break;  // Early exit
}

$iterator->close();
```

## Installation

### Node.js

```bash
npm install cisv
# or build from source
cd bindings/nodejs && npm install && npm run build
```

### Python (nanobind - recommended)

```bash
pip install cisv
# or build from source
cd bindings/python-nanobind && pip install .
```

### PHP

```bash
cd bindings/php
phpize
./configure
make
sudo make install
```

## Performance

All bindings use the same SIMD-optimized C core, providing near-native performance:

| Binding | Overhead vs C | Notes |
|---------|--------------|-------|
| Node.js (N-API) | ~5-10% | Zero-copy where possible |
| Python (nanobind) | ~5-15% | GIL released during parsing |
| PHP | ~5-10% | Native extension |

The iterator API is particularly efficient for:
- Processing files larger than available memory
- Early termination scenarios (finding specific rows)
- Streaming data processing pipelines

## Contributing

When adding features to bindings:

1. Implement in the C core first (`core/`)
2. Add bindings to all supported languages
3. Include tests and documentation
4. Update this README with feature comparison

## License

MIT - See [LICENSE](../LICENSE) for details.
