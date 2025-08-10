# CISV

![Performance](https://img.shields.io/badge/performance-blazing%20fast-red)
![SIMD](https://img.shields.io/badge/SIMD-AVX512%2FAVX2-green)
![License](https://img.shields.io/badge/license-MIT-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)

High-performance CSV parser and writer leveraging SIMD instructions and zero-copy memory mapping. Available as both a Node.js native addon and standalone CLI tool.

## PERFORMANCE

- **469,968 MB/s** throughput on 2M row CSV files (AVX-512)
- **10-100x faster** than popular CSV parsers
- Zero-copy memory-mapped I/O with kernel optimizations
- SIMD accelerated with AVX-512/AVX2 auto-detection

## INSTALLATION

### NODE.JS PACKAGE
```bash
npm install cisv
```

### CLI TOOL (FROM SOURCE)
```bash
git clone https://github.com/sanix-darker/cisv
cd cisv
make cli
sudo make install-cli
```

### BUILD FROM SOURCE (NODE.JS ADDON)
```bash
npm install -g node-gyp
make build
```

## QUICK START

### NODE.JS
```javascript
const { cisvParser } = require('cisv');

const parser = new cisvParser();
const rows = parser.parseSync('./data.csv');
console.log(`Parsed ${rows.length} rows`);
```

### CLI

```bash
# Count rows
cisv -c large_file.csv

# Select columns
cisv -s 0,2,5 data.csv

# First 100 rows
cisv --head 100 data.csv
```

## API REFERENCE

### TYPESCRIPT DEFINITIONS
```typescript
interface ParsedRow extends Array<string> {}
interface ParseStats {
   rowCount: number;
   fieldCount: number;
   totalBytes: number;
   parseTime: number;
}
interface TransformInfo {
   cTransformCount: number;
   jsTransformCount: number;
   fieldIndices: number[];
}
```

### BASIC PARSING
```javascript
const parser = new cisv.cisvParser();

// Synchronous
const rows = parser.parseSync('data.csv');

// Asynchronous
const asyncRows = await parser.parse('large-file.csv');

// From string
const csvString = 'name,age,city\nJohn,30,NYC\nJane,25,LA';
const stringRows = parser.parseString(csvString);
```

### STREAMING
```javascript
const streamParser = new cisv.cisvParser();
const stream = fs.createReadStream('huge-file.csv');

stream.on('data', chunk => streamParser.write(chunk));
stream.on('end', () => {
    streamParser.end();
    const results = streamParser.getRows();
});
```

### DATA TRANSFORMATION

Built-in C transforms (optimized):
```javascript
parser
    .transform(0, 'uppercase')      // Column 0 to uppercase
    .transform(1, 'lowercase')       // Column 1 to lowercase
    .transform(2, 'trim')           // Column 2 trim whitespace
    .transform(3, 'to_int')         // Column 3 to integer
    .transform(4, 'to_float')       // Column 4 to float
    .transform(5, 'base64_encode')  // Column 5 to base64
    .transform(6, 'hash_sha256');   // Column 6 to SHA256
```

Custom JavaScript transforms:
```javascript
// Single field
parser.transform(7, value => new Date(value).toISOString());

// All fields
parser.transform(-1, value => value.replace(/[^\w\s]/gi, ''));

// Chain transforms
parser
    .transform(0, 'trim')
    .transform(0, 'uppercase')
    .transform(0, val => val.substring(0, 10));
```

### CONFIGURATION
```javascript
const customParser = new cisv.cisvParser({
    delimiter: '|',
    quote: "'",
    escape: '\\',
    headers: true,
    skipEmptyLines: true
});
```

## CLI USAGE

### PARSING
```bash
cisv [OPTIONS] [FILE]

Options:
  -h, --help              Show help message
  -v, --version           Show version
  -d, --delimiter DELIM   Field delimiter (default: ,)
  -s, --select COLS       Select columns (comma-separated indices)
  -c, --count            Show only row count
  --head N               Show first N rows
  --tail N               Show last N rows
  -o, --output FILE      Write to FILE instead of stdout
  -b, --benchmark        Run benchmark mode
```

### WRITING

```bash
cisv write [OPTIONS]

Options:
  -g, --generate N       Generate N rows of test data
  -o, --output FILE      Output file
  -d, --delimiter DELIM  Field delimiter
  -Q, --quote-all        Quote all fields
  -r, --crlf             Use CRLF line endings
  -n, --null TEXT        Null representation
  -b, --benchmark        Benchmark mode
```

## BENCHMARKS

### PARSER PERFORMANCE (273 MB, 5M ROWS)

| Parser        | Speed (MB/s) | Time (ms) | Relative       |
|---------------|--------------|-----------|----------------|
| **cisv**      | 7,184        | 38        | 1.0x (fastest) |
| rust-csv      | 391          | 698       | 18x slower     |
| xsv           | 650          | 420       | 11x slower     |
| csvkit        | 28           | 9,875     | 260x slower    |

### NODE.JS LIBRARY BENCHMARKS

- **Synchronous with Data Access:**

| Library            | Speed (MB/s) | Operations/sec |
|--------------------|--------------|----------------|
| cisv              | 61.24        | 136,343        |
| csv-parse         | 15.48        | 34,471         |
| papaparse         | 25.67        | 57,147         |

- **Asynchronous Streaming:**

| Library            | Speed (MB/s) | Operations/sec |
|--------------------|--------------|----------------|
| cisv              | 76.94        | 171,287        |
| papaparse         | 16.54        | 36,815         |
| neat-csv          | 8.11         | 18,055         |

### RUNNING BENCHMARKS

```bash
# CLI benchmarks
make clean && make cli && make benchmark-cli

# Node.js benchmarks
npm run benchmark

# Docker isolated benchmarks
docker build -t cisv-benchmark .
docker run --rm --cpus="2.0" --memory="4g" cisv-benchmark
```

## TECHNICAL ARCHITECTURE

- **SIMD Processing**: AVX-512 (64-byte vectors) or AVX2 (32-byte vectors) for parallel processing
- **Memory Mapping**: Direct kernel-to-userspace zero-copy with `mmap()`
- **Optimized Buffering**: 1MB ring buffer sized for L3 cache efficiency
- **Compiler Optimizations**: LTO and architecture-specific tuning with `-march=native`

## FEATURES

- RFC 4180 compliant
- Handles quoted fields with embedded delimiters
- Streaming API for unlimited file sizes
- Cross-platform (Linux, macOS, Windows via WSL)
- Safe fallback for non-x86 architectures
- High-performance CSV writer with SIMD optimization

## CONTRIBUTING

Areas of interest:
- ARM NEON/SVE support
- Windows native support
- Parallel parsing for multi-core systems
- Custom memory allocators
- Streaming compression support

## LICENSE

GPL2 © [sanix-darker](https://github.com/sanix-darker)

## ACKNOWLEDGMENTS

Inspired by:
- [simdjson](https://github.com/simdjson/simdjson) - Parsing gigabytes of JSON per second
- [xsv](https://github.com/BurntSushi/xsv) - Fast CSV command line toolkit
- [rust-csv](https://github.com/BurntSushi/rust-csv) - CSV parser for Rust
