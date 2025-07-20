# CISV

![Performance Benchmark](https://img.shields.io/badge/performance-blazing%20fast-red)
![SIMD Support](https://img.shields.io/badge/SIMD-AVX512%2FAVX2-green)
![License](https://img.shields.io/badge/license-MIT-blue)

The horribly fastest CSV parser you will ever see, leveraging native SIMD instructions and zero-copy memory mapping for unparalleled performance.

## FEATURES

### CORE PERFORMANCE

- **SIMD Acceleration**: AVX-512/AVX2 dual-path with compile-time selection (64-byte vectorization)
- **Zero-Copy Architecture**: Memory-mapped I/O with `MADV_SEQUENTIAL` + `MADV_HUGEPAGE` optimizations
- **Efficient Buffering**: 1MiB aligned ring buffer sized for LLC residency
- **Portable Fallback**: Scalar parser for non-x86 architectures

### ENGINEERING EXCELLENCE

- **Memory Safe**: Over-read-safe padding ensures vector loads never fault
- **Streaming Optimized**: Heuristic flushing balances latency and throughput
- **Compiler Optimized**: LTO (-flto) and -march=native for architecture-specific tuning
- **Robust Code**: Strict C11 compliance with Wall/Wextra warnings

## INSTALLATION

### INSTALL FROM NPM

```bash
npm install cisv
```

### BUILD FROM SOURCE

```bash
# Install build tools
npm install -g node-gyp
npm install -g node-addon-api --save-dev

# Build the native addon (auto-detects AVX-512/AVX2 support)
make
```

## USAGE EXAMPLES

### BASIC SYNCHRONOUS PARSING

```javascript
const { cisvParser } = require('cisv');
const parser = new cisvParser();

// Parse entire file synchronously
const rows = parser.parseSync('./fixtures/data.csv');
console.log(`Parsed ${rows.length} rows`);
```

### STREAMING LARGE FILES

```javascript
const { cisvParser } = require('cisv');
const fs = require('fs');

const parser = new cisvParser();
const stream = fs.createReadStream('./fixtures/large.csv');

stream.on('data', chunk => parser.write(chunk));
stream.on('end', () => {
  parser.end();
  const rows = parser.getRows();
  console.log(`Processed ${rows.length} rows`);
});
```

### Advanced: Progress Reporting

```javascript
const parser = new cisvParser();
const stats = fs.statSync('./huge.csv');
let bytesProcessed = 0;

fs.createReadStream('./huge.csv')
  .on('data', chunk => {
    bytesProcessed += chunk.length;
    parser.write(chunk);
    console.log(`Progress: ${(bytesProcessed/stats.size*100).toFixed(1)}%`);
  })
  .on('end', () => {
    const results = parser.getRows();
    // Process results...
  });
```

## BENCHMARK

NOTE: To run benchmarks yourself, make sure to add these dependencies to the `package.json` to run the next scripts by your self.

```json
  "dependencies": {
    "benchmark": "^2.1.4",
    "csv-parse": "^6.0.0",
    "fast-csv": "^5.0.2",
    "neat-csv": "^7.0.0",
    "papaparse": "^5.5.3"
  }
```

### ON 10 ROWS

```console
$ node ./benchmark/benchmark.js
Starting benchmark with file: ./data.csv
All tests will retrieve row index: 10

File size: 0.00 MB
Sample of target row: [ '10', 'Jane Smith', 'jane.smith@email.com', 'Los Angeles' ]

--------------------------------------------------

### Synchronous Results

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 47.61        | 0.01          | 105992         |
| csv-parse (sync)   | 35.96        | 0.01          | 80062          |
| papaparse (sync)   | 56.51        | 0.01          | 125801         |


### Asynchronous Results

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 46.68        | 0.01          | 103916         |
| papaparse (async/stream) | 44.18        | 0.01          | 98349          |
| neat-csv (async/promise) | 23.64        | 0.02          | 52621          |
```

### ON 2MILLIONS ROWS

Given :

```bash
$ cat ./customers-2_000_000__rows.csv | wc -l
 2000000
```


```console
$ node ./benchmark/benchmark.js ./customers-2_000_000__rows.csv
 Starting benchmark with file: ./customers-2_000_000__rows.csv
 All tests will retrieve row index: 1040

File size: 333.23 MB
Sample of target row: [
  '1040',
  'C3E4e3a9bc1928d',
  'Dale',
  'Hoffman',
  'Stuart-Jordan',
  'West Howard',
  'Malaysia',
  '(059)003-8279',
  '(265)374-1884x5411',
  'gfry@huerta.com',
  '2020-04-13',
  'https://www.duke.com/'
]

### Synchronous Results

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 469968048.48 | 0.00          | 1410342        |
| csv-parse (sync)   | 42.27        | 7884.01       | 0              |
| papaparse (sync)   | 215.09       | 1549.28       | 1              |


### Asynchronous Results

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 22783832.74  | 0.01          | 68373          |
| papaparse (async/stream) | 165.29       | 2015.97       | 0              |
| neat-csv (async/promise) | 64.53        | 5163.86       | 0              |
```

## Development

### Build System
```bash
make        # Standard build
make debug  # Debug build
make test   # Run all tests
make perf   # Performance tests
make clean  # Clean build artifacts
```

### Testing
We provide comprehensive test coverage:
```bash
npm test  # Runs unit and integration tests

# Test specific components
npm run test:sync     # Synchronous API tests
npm run test:stream   # Streaming API tests
npm run test:memory   # Memory management tests
```

## Performance Tips

1. **File Size**: For files >100MB, use streaming API
2. **Memory**: Ensure sufficient RAM for memory mapping
3. **CPU**: Enable AVX-512 in BIOS for maximum performance
4. **OS**: Linux with transparent hugepages recommended

## Roadmap

- [x] AVX-512/AVX2 support
- [x] Zero-copy memory mapping
- [ ] Windows memory-mapped I/O support
- [ ] ARM NEON/ASIMD optimization
- [ ] Parallel parsing mode
- [ ] Fix double quotes squashing

## CONTRIBUTING

We welcome performance optimizations and platform support contributions. Please ensure:

1. All tests pass (`make test`)
2. Benchmarks show no regressions
3. Code follows our style guide

## LICENSE

MIT © [sanix-darker]
