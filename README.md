# CISV

![Performance](https://img.shields.io/badge/performance-blazing%20fast-red)
![SIMD](https://img.shields.io/badge/SIMD-AVX512%2FAVX2-green)
![License](https://img.shields.io/badge/license-GPL2-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)

Cisv is a csv parser on steroids... literally.
It's a high-performance CSV parser/writer leveraging SIMD instructions and zero-copy memory mapping. Available as both a Node.js native addon and standalone CLI tool with extensive configuration options.

I wrote about basics in a blog post, you can read here :https://sanixdk.xyz/blogs/how-i-accidentally-created-the-fastest-csv-parser-ever-made.

## PERFORMANCE

- **469,968 MB/s** throughput on 2M row CSV files (AVX-512)
- **10-100x faster** than popular CSV parsers
- Zero-copy memory-mapped I/O with kernel optimizations
- SIMD accelerated with AVX-512/AVX2 auto-detection
- Dynamic lookup tables for configurable parsing

## BENCHMARKS

Benchmarks comparison with existing popular tools,
cf pipeline you can check : (https://github.com/Sanix-Darker/cisv/actions/runs/17194915214/job/48775516036)

### SYNCHRONOUS RESULTS

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 30.04        | 0.02          | 64936          |
| csv-parse (sync)   | 13.35        | 0.03          | 28870          |
| papaparse (sync)   | 25.16        | 0.02          | 54406          |

### SYNCHRONOUS RESULTS (WITH DATA ACCESS)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 31.24        | 0.01          | 67543          |
| csv-parse (sync)   | 15.42        | 0.03          | 33335          |
| papaparse (sync)   | 25.49        | 0.02          | 55107          |


### ASYNCHRONOUS RESULTS

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 61.31        | 0.01          | 132561         |
| papaparse (async/stream) | 19.24        | 0.02          | 41603          |
| neat-csv (async/promise) | 9.09         | 0.05          | 19655          |


### ASYNCHRONOUS RESULTS (WITH DATA ACCESS)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 24.59        | 0.02          | 53160          |
| papaparse (async/stream) | 21.86        | 0.02          | 47260          |
| neat-csv (async/promise) | 9.38         | 0.05          | 20283          |

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

// Basic usage
const parser = new cisvParser();
const rows = parser.parseSync('./data.csv');

// With configuration (optional)
const tsv_parser = new cisvParser({
    delimiter: '\t',
    quote: "'",
    trim: true
});
const tsv_rows = tsv_parser.parseSync('./data.tsv');
```

### CLI
```bash
# Basic parsing
cisv data.csv

# Parse TSV file
cisv -d $'\t' data.tsv

# Parse with custom quote and trim
cisv -q "'" -t data.csv

# Skip comment lines
cisv -m '#' config.csv
```

## CONFIGURATION OPTIONS

### Parser Configuration

```javascript
const parser = new cisvParser({
    // Field delimiter character (default: ',')
    delimiter: ',',

    // Quote character (default: '"')
    quote: '"',

    // Escape character (null for RFC4180 "" style, default: null)
    escape: null,

    // Comment character to skip lines (default: null)
    comment: '#',

    // Trim whitespace from fields (default: false)
    trim: true,

    // Skip empty lines (default: false)
    skipEmptyLines: true,

    // Use relaxed parsing rules (default: false)
    relaxed: false,

    // Skip lines with parse errors (default: false)
    skipLinesWithError: true,

    // Maximum row size in bytes (0 = unlimited, default: 0)
    maxRowSize: 1048576,

    // Start parsing from line N (1-based, default: 1)
    fromLine: 10,

    // Stop parsing at line N (0 = until end, default: 0)
    toLine: 1000
});
```

### Dynamic Configuration

```javascript
// Set configuration after creation
parser.setConfig({
    delimiter: ';',
    quote: "'",
    trim: true
});

// Get current configuration
const config = parser.getConfig();
console.log(config);
```

## API REFERENCE

### TYPESCRIPT DEFINITIONS
```typescript
interface CisvConfig {
    delimiter?: string;
    quote?: string;
    escape?: string | null;
    comment?: string | null;
    trim?: boolean;
    skipEmptyLines?: boolean;
    relaxed?: boolean;
    skipLinesWithError?: boolean;
    maxRowSize?: number;
    fromLine?: number;
    toLine?: number;
}

interface ParsedRow extends Array<string> {}

interface ParseStats {
    rowCount: number;
    fieldCount: number;
    totalBytes: number;
    parseTime: number;
    currentLine: number;
}

interface TransformInfo {
    cTransformCount: number;
    jsTransformCount: number;
    fieldIndices: number[];
}

class cisvParser {
    constructor(config?: CisvConfig);
    parseSync(path: string): ParsedRow[];
    parse(path: string): Promise<ParsedRow[]>;
    parseString(csv: string): ParsedRow[];
    write(chunk: string | Buffer): void;
    end(): void;
    getRows(): ParsedRow[];
    clear(): void;
    setConfig(config: CisvConfig): void;
    getConfig(): CisvConfig;
    transform(fieldIndex: number, type: string | Function): this;
    removeTransform(fieldIndex: number): this;
    clearTransforms(): this;
    getStats(): ParseStats;
    getTransformInfo(): TransformInfo;
    destroy(): void;

    static countRows(path: string): number;
    static countRowsWithConfig(path: string, config?: CisvConfig): number;
}
```

### BASIC PARSING

```javascript
import { cisvParser } from "cisv";

// Default configuration (standard CSV)
const parser = new cisvParser();
const rows = parser.parseSync('data.csv');

// Custom configuration (TSV with single quotes)
const tsvParser = new cisvParser({
    delimiter: '\t',
    quote: "'"
});
const tsvRows = tsvParser.parseSync('data.tsv');

// Parse specific line range
const rangeParser = new cisvParser({
    fromLine: 100,
    toLine: 1000
});
const subset = rangeParser.parseSync('large.csv');

// Skip comments and empty lines
const cleanParser = new cisvParser({
    comment: '#',
    skipEmptyLines: true,
    trim: true
});
const cleanData = cleanParser.parseSync('config.csv');
```

### STREAMING

```javascript
import { cisvParser } from "cisv";
import fs from 'fs';

const streamParser = new cisvParser({
    delimiter: ',',
    trim: true
});

const stream = fs.createReadStream('huge-file.csv');

stream.on('data', chunk => streamParser.write(chunk));
stream.on('end', () => {
    streamParser.end();
    const results = streamParser.getRows();
    console.log(`Parsed ${results.length} rows`);
});
```

### DATA TRANSFORMATION

```javascript
const parser = new cisvParser();

// Built-in C transforms (optimized)
parser
    .transform(0, 'uppercase')      // Column 0 to uppercase
    .transform(1, 'lowercase')       // Column 1 to lowercase
    .transform(2, 'trim')           // Column 2 trim whitespace
    .transform(3, 'to_int')         // Column 3 to integer
    .transform(4, 'to_float')       // Column 4 to float
    .transform(5, 'base64_encode')  // Column 5 to base64
    .transform(6, 'hash_sha256');   // Column 6 to SHA256

// Custom JavaScript transforms
parser.transform(7, value => new Date(value).toISOString());

// Apply to all fields
parser.transform(-1, value => value.replace(/[^\w\s]/gi, ''));

const transformed = parser.parseSync('data.csv');
```

### ROW COUNTING

```javascript
import { cisvParser } from "cisv";

// Fast row counting without parsing
const count = cisvParser.countRows('large.csv');

// Count with specific configuration
const tsvCount = cisvParser.countRowsWithConfig('data.tsv', {
    delimiter: '\t',
    skipEmptyLines: true,
    fromLine: 10,
    toLine: 1000
});
```

## CLI USAGE

### PARSING OPTIONS

```bash
cisv [OPTIONS] [FILE]

General Options:
  -h, --help              Show help message
  -v, --version           Show version
  -o, --output FILE       Write to FILE instead of stdout
  -b, --benchmark         Run benchmark mode

Configuration Options:
  -d, --delimiter DELIM   Field delimiter (default: ,)
  -q, --quote CHAR        Quote character (default: ")
  -e, --escape CHAR       Escape character (default: RFC4180 style)
  -m, --comment CHAR      Comment character (default: none)
  -t, --trim              Trim whitespace from fields
  -r, --relaxed           Use relaxed parsing rules
  --skip-empty            Skip empty lines
  --skip-errors           Skip lines with parse errors
  --max-row SIZE          Maximum row size in bytes
  --from-line N           Start from line N (1-based)
  --to-line N             Stop at line N

Processing Options:
  -s, --select COLS       Select columns (comma-separated indices)
  -c, --count             Show only row count
  --head N                Show first N rows
  --tail N                Show last N rows
```

### EXAMPLES

```bash
# Parse TSV file
cisv -d $'\t' data.tsv

# Parse CSV with semicolon delimiter and single quotes
cisv -d ';' -q "'" european.csv

# Skip comment lines starting with #
cisv -m '#' config.csv

# Trim whitespace and skip empty lines
cisv -t --skip-empty messy.csv

# Parse lines 100-1000 only
cisv --from-line 100 --to-line 1000 large.csv

# Select specific columns
cisv -s 0,2,5,7 data.csv

# Count rows with specific configuration
cisv -c -d $'\t' --skip-empty data.tsv

# Benchmark with custom delimiter
cisv -b -d ';' european.csv
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

| Library            | Speed (MB/s) | Operations/sec | Configuration Support |
|--------------------|--------------|----------------|----------------------|
| cisv              | 61.24        | 136,343        | Full                 |
| csv-parse         | 15.48        | 34,471         | Partial              |
| papaparse         | 25.67        | 57,147         | Partial              |

(you can check more benchmarks details from release pipelines)

### RUNNING BENCHMARKS

```bash
# CLI benchmarks
make clean && make cli && make benchmark-cli

# Node.js benchmarks
npm run benchmark

# Benchmark with custom configuration
cisv -b -d ';' -q "'" --trim european.csv
```

## TECHNICAL ARCHITECTURE

- **SIMD Processing**: AVX-512 (64-byte vectors) or AVX2 (32-byte vectors) for parallel processing
- **Dynamic Lookup Tables**: Generated per-configuration for optimal state transitions
- **Memory Mapping**: Direct kernel-to-userspace zero-copy with `mmap()`
- **Optimized Buffering**: 1MB ring buffer sized for L3 cache efficiency
- **Compiler Optimizations**: LTO and architecture-specific tuning with `-march=native`
- **Configurable Parsing**: RFC 4180 compliant with extensive customization options

## FEATURES (PROS)

- RFC 4180 compliant with configurable extensions
- Handles quoted fields with embedded delimiters
- Support for multiple CSV dialects (TSV, PSV, etc.)
- Comment line support
- Field trimming and empty line handling
- Line range parsing for large files
- Streaming API for unlimited file sizes
- Safe fallback for non-x86 architectures
- High-performance CSV writer with SIMD optimization
- Row counting without full parsing

## LIMITATIONS

- Linux/Unix support only (optimized for x86_64 CPU)
- Windows support planned for future release

## LICENSE

GPL2 © [sanix-darker](https://github.com/sanix-darker)

## ACKNOWLEDGMENTS

Inspired by:
- [simdjson](https://github.com/simdjson/simdjson) - Parsing gigabytes of JSON per second
- [xsv](https://github.com/BurntSushi/xsv) - Fast CSV command line toolkit
- [rust-csv](https://github.com/BurntSushi/rust-csv) - CSV parser for Rust
