# CISV

![License](https://img.shields.io/badge/license-MIT-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Sanix-Darker/cisv)

High-performance CSV parser and writer with SIMD optimizations (AVX-512/AVX2). Available as a C library, CLI tool, and bindings for Node.js, Python, and PHP.

Blog post: [How I accidentally created the fastest CSV parser ever made](https://sanixdk.xyz/blogs/how-i-accidentally-created-the-fastest-csv-parser-ever-made)

## FEATURES

- SIMD-accelerated parsing (AVX-512, AVX2, with scalar fallback)
- Zero-copy memory-mapped file processing
- Streaming API for large files
- Row-by-row iterator API (fgetcsv-style) with early exit support
- Configurable delimiters, quotes, escapes, and comments
- Field transforms (uppercase, lowercase, trim, hash, base64, etc.)
- Parallel chunk processing for multi-threaded parsing
- RFC 4180 compliant with relaxed mode option
- CSV writer with SIMD optimization

## PERFORMANCE

Benchmarks on Intel Core i5-8365U:

| Test | Throughput |
|------|------------|
| Simple CSV | 165-290 MB/s |
| Quoted fields | 175-189 MB/s |
| Large fields | 153-169 MB/s |

For detailed benchmarks, see [BENCHMARKS.md](./BENCHMARKS.md).

## INSTALLATION

### BUILD FROM SOURCE

```bash
git clone https://github.com/sanix-darker/cisv
cd cisv
make all
```

### INSTALL CLI

```bash
sudo make install-cli
```

### NODE.JS

```bash
cd bindings/nodejs
npm install
npm run build
```

### PYTHON

```bash
cd bindings/python
pip install -e .
```

### PHP

```bash
cd bindings/php
phpize
./configure
make
sudo make install
```

## CLI USAGE

```bash
cisv [OPTIONS] FILE

Options:
  -d, --delimiter CHAR   Field delimiter (default: ,)
  -q, --quote CHAR       Quote character (default: ")
  -e, --escape CHAR      Escape character
  -m, --comment CHAR     Comment line prefix
  -t, --trim             Trim whitespace from fields
  -r, --relaxed          Relaxed parsing mode
  -c, --count            Count rows only
  -s, --select COLS      Select columns by index
  -o, --output FILE      Output file
  -b, --benchmark        Benchmark mode
  --skip-empty           Skip empty lines
  --skip-errors          Skip lines with errors
  --from-line N          Start at line N
  --to-line N            Stop at line N
  --head N               First N rows
  --tail N               Last N rows
```

### EXAMPLES

```bash
# Parse CSV
cisv data.csv

# Parse TSV
cisv -d $'\t' data.tsv

# Count rows
cisv -c data.csv

# Select columns 0, 2, 5
cisv -s 0,2,5 data.csv

# Benchmark
cisv -b data.csv

# Parse lines 100-500
cisv --from-line 100 --to-line 500 data.csv
```

### CSV WRITER

```bash
cisv write [OPTIONS]

Options:
  -g, --generate N       Generate N test rows
  -o, --output FILE      Output file
  -d, --delimiter CHAR   Field delimiter
  -Q, --quote-all        Quote all fields
  -r, --crlf             Use CRLF line endings
  -n, --null TEXT        NULL representation
```

## DOCKER BENCHMARKS

```bash
docker build -t cisv-benchmark .
docker run --rm --cpus="2.0" --memory="4g" cisv-benchmark
```

## C API

```c
#include <cisv/parser.h>
#include <cisv/writer.h>
#include <cisv/transformer.h>

// Callbacks
void on_field(void *user, const char *data, size_t len);
void on_row(void *user);

// Parse file
cisv_config cfg;
cisv_config_init(&cfg);
cfg.field_cb = on_field;
cfg.row_cb = on_row;
cfg.delimiter = ',';

cisv_parser *p = cisv_parser_create_with_config(&cfg);
cisv_parser_parse_file(p, "data.csv");
cisv_parser_destroy(p);

// Count rows (fast mode)
size_t count = cisv_parser_count_rows("data.csv");

// Row-by-row iterator (fgetcsv-style, supports early exit)
cisv_iterator_t *it = cisv_iterator_open("data.csv", &cfg);
const char **fields;
const size_t *lengths;
size_t field_count;
while (cisv_iterator_next(it, &fields, &lengths, &field_count) == CISV_ITER_OK) {
    // Process row - fields valid until next call
    if (should_stop) break;  // Early exit supported
}
cisv_iterator_close(it);

// Parallel processing
cisv_mmap_file_t *file = cisv_mmap_open("data.csv");
int chunk_count;
cisv_chunk_t *chunks = cisv_split_chunks(file, 4, &chunk_count);
// Parse chunks in parallel threads
cisv_mmap_close(file);
```

## NODE.JS API

```javascript
const { cisvParser } = require('cisv');

// Parse file
const parser = new cisvParser({ delimiter: ',', quote: '"', trim: true });
const rows = parser.parseSync('data.csv');

// Parse with transforms
parser.transform(0, 'uppercase')
      .transform(1, 'trim')
      .transform(2, (value) => value.toUpperCase());
const transformedRows = parser.parseSync('data.csv');

// Stream parsing
const streamParser = new cisvParser();
streamParser.write(chunk);
streamParser.end();
const results = streamParser.getRows();

// Row-by-row iteration (memory efficient, supports early exit)
const iterParser = new cisvParser({ delimiter: ',' });
iterParser.openIterator('large.csv');
let row;
while ((row = iterParser.fetchRow()) !== null) {
    console.log(row);
    if (row[0] === 'stop') break;  // Early exit
}
iterParser.closeIterator();
```

## PYTHON API

```python
import cisv

# Parse file
rows = cisv.parse_file('data.csv', delimiter=',', trim=True)

# Parse string
rows = cisv.parse_string('a,b,c\n1,2,3')

# Count rows (fast)
count = cisv.count_rows('data.csv')

# Row-by-row iteration (memory efficient, supports early exit)
with cisv.CisvIterator('large.csv') as reader:
    for row in reader:
        print(row)
        if row[0] == 'stop':
            break  # Early exit - no wasted work

# Or use the convenience function
for row in cisv.open_iterator('data.csv'):
    process(row)
```

## PHP API

```php
<?php
$parser = new Cisv\Parser([
    'delimiter' => ',',
    'quote' => '"',
    'trim' => true
]);

$rows = $parser->parseFile('data.csv');

// With transforms
$parser->addTransform(0, 'uppercase');
$parser->addTransform(1, 'trim');
$rows = $parser->parseFile('data.csv');
```

## TRANSFORMS

Available transforms for field processing:

| Transform | Description |
|-----------|-------------|
| uppercase | Convert to uppercase |
| lowercase | Convert to lowercase |
| trim | Remove leading/trailing whitespace |
| trim_left | Remove leading whitespace |
| trim_right | Remove trailing whitespace |
| to_int | Parse as integer |
| to_float | Parse as float |
| to_bool | Parse as boolean |
| hash_md5 | MD5 hash |
| hash_sha256 | SHA-256 hash |
| base64_encode | Base64 encode |
| base64_decode | Base64 decode |
| url_encode | URL encode |
| url_decode | URL decode |

## CONFIGURATION OPTIONS

| Option | Default | Description |
|--------|---------|-------------|
| delimiter | , | Field separator |
| quote | " | Quote character |
| escape | (RFC4180) | Escape character |
| comment | none | Comment line prefix |
| trim | false | Trim field whitespace |
| skip_empty_lines | false | Skip empty lines |
| relaxed | false | Relaxed parsing mode |
| max_row_size | 0 | Max row size (0 = unlimited) |
| from_line | 0 | Start line (1-based) |
| to_line | 0 | End line (0 = all) |
| skip_lines_with_error | false | Skip error lines |

## BUILDING WITH PGO

Profile-Guided Optimization for best performance:

```bash
# Generate profile
cd core/build
cmake -DCISV_PGO_GENERATE=ON ..
make
./cisv -b large_file.csv

# Build with profile
cmake -DCISV_PGO_USE=ON ..
make
```

## ARCHITECTURE

- SIMD: AVX-512 (64-byte) or AVX2 (32-byte) vector processing
- Memory: Zero-copy mmap with kernel read-ahead hints
- Buffer: 1MB ring buffer sized for L3 cache
- Fallback: SWAR (SIMD Within A Register) for non-AVX systems
- Security: Integer overflow protection, bounds checking

## REQUIREMENTS

- Linux/macOS (x86_64 or ARM64)
- GCC 7+ or Clang 6+
- CMake 3.14+
- Node.js 14+ (for Node.js binding)
- Python 3.7+ (for Python binding)
- PHP 7.4+ (for PHP extension)

## LIMITATIONS

- Windows support planned for future release (maybe...)

## LICENSE

MIT

## AUTHOR

[sanix-darker](https://github.com/sanix-darker)

## ACKNOWLEDGMENTS

Inspired by:
- [simdjson](https://github.com/simdjson/simdjson) - Parsing gigabytes of JSON per second
- [xsv](https://github.com/BurntSushi/xsv) - Fast CSV command line toolkit
- [rust-csv](https://github.com/BurntSushi/rust-csv) - CSV parser for Rust
