# CISV

![Performance](https://img.shields.io/badge/performance-blazing%20fast-red)
![SIMD](https://img.shields.io/badge/SIMD-AVX512%2FAVX2-green)
![License](https://img.shields.io/badge/license-MIT-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)

> **cisv** is a high-performance CSV parser and writer that leverages SIMD instructions and zero-copy memory mapping to achieve unparalleled parsing and writing speeds. Available as both a Node.js native addon and a standalone CLI tool.

### DISCLAIMER

- The reader/parser is much more optimized than the writer,
- This project is on hightly dev mode, don't assume everything will not break.

### PERFORMANCE

- **469,968 MB/s** throughput on 2M row CSV files (AVX-512)
- **10-100x faster** than popular CSV parsers
- **Zero-copy** memory-mapped I/O with kernel optimizations
- **SIMD accelerated** with AVX-512/AVX2 auto-detection

### BENCHMARKS

For this given command :

```console
$ make clean && make cli && cargo install qsv && make install-benchmark-deps && make benchmark-cli
```

We have this representation in terms of performances :

- on 5M ROWs CSV (273Mb)

Based on the provided benchmark data for a 273 MB CSV file with 5 million rows, here's the performance comparison table in markdown format:

| Parser        | Speed (MB/s) | Time (ms) | Relative       |
|---------------|--------------|-----------|----------------|
| **cisv**      | 7,184        | 38        | 1.0x           |
| rust-csv      | 391          | 698       | 18x slower     |
| xsv           | 650          | 420       | 11x slower     |
| csvkit        | 28           | 9,875     | 260x slower    |

## BENCHMARK RESULTS FOR ROW COUNTING TEST

```console
# by running :
$ bash ./benchmark_cli.sh
```

### TEST FILES

| File | Rows | Size |
|------|------|------|
| small.csv | 1,000 | 84 KB |
| medium.csv | 100,000 | 9.2 MB |
| large.csv | 1,000,000 | 98 MB |
| xlarge.csv | 10,000,000 | 1.1 GB |

### Benchmark Results for small.csv (1K rows, 84 KB)

| Tool | Time (seconds) | Time (ms) | Memory (KB) | Relative Speed | Status |
|------|----------------|-----------|-------------|----------------|---------|
| **cisv (baseline)** | 0.003314527 | 3.31 | 3,648 | 1.0x | ✓ |
| **wc -l** | 0.002764321 | 2.76 | 3,688 | 0.83x faster | ✓ |
| **miller** | 0.003727007 | 3.73 | 3,728 | 1.12x slower | ❌ Error |
| **rust-csv** | 0.004020831 | 4.02 | 3,660 | 1.21x slower | ✓ |
| **xsv** | 0.013443824 | 13.44 | 5,944 | 4.06x slower | ✓ |
| **csvkit** | 0.313832731 | 313.83 | 25,564 | 94.7x slower | ✓ |

*Note: The benchmark script runs the same tests on medium.csv, large.csv, and xlarge.csv. For complete results, you would need to capture the output for all file sizes. Each test is run 3 times and the total time is reported.*

## CSV WRITER BENCHMARK RESULTS

```console
# by running :
$ bash ./benchmark_cli_writer.sh
```

### Writer Performance Comparison

#### Small (1K rows, 50 KB)

| Tool | Time (s) | Throughput (MB/s) | Rows/sec | Relative |
|------|----------|-------------------|----------|----------|
| **C fprintf** | 0.003 | 16.85 | 337,140 | 1.0x (fastest) |
| **awk** | 0.005 | 10.77 | 215,402 | 1.6x slower |
| **cisv write** | 0.007 | 6.97 | 139,454 | 2.4x slower |
| **Python csv** | 0.028 | 1.77 | 35,523 | 9.5x slower |

#### Medium (100K rows, 6.46 MB)

| Tool | Time (s) | Throughput (MB/s) | Rows/sec | Relative |
|------|----------|-------------------|----------|----------|
| **C fprintf** | 0.057 | 112.73 | 1,745,141 | 1.0x (fastest) |
| **awk** | 0.199 | 32.39 | 501,544 | 3.5x slower |
| **cisv write** | 0.203 | 31.80 | 492,326 | 3.5x slower |
| **Python csv** | 0.265 | 24.71 | 377,888 | 4.6x slower |

#### Large (1M rows, 68.43 MB)

| Tool | Time (s) | Throughput (MB/s) | Rows/sec | Relative |
|------|----------|-------------------|----------|----------|
| **C fprintf** | 0.611 | 111.96 | 1,636,174 | 1.0x (fastest) |
| **cisv write** | 1.970 | 34.74 | 507,689 | 3.2x slower |
| **awk** | 2.139 | 31.98 | 467,476 | 3.5x slower |
| **Python csv** | 2.568 | 26.98 | 389,422 | 4.2x slower |

#### XLarge (10M rows, 722.53 MB)

| Tool | Time (s) | Throughput (MB/s) | Rows/sec | Relative |
|------|----------|-------------------|----------|----------|
| **C fprintf** | 6.625 | 109.06 | 1,509,538 | 1.0x (fastest) |
| **cisv write** | 20.205 | 35.76 | 494,927 | 3.0x slower |
| **awk** | 22.596 | 31.97 | 442,558 | 3.4x slower |
| **Python csv** | 27.493 | 26.59 | 363,726 | 4.1x slower |

### EXPECTED PERFORMANCE SCALING

Based on the file sizes:

- **medium.csv**: ~110x larger than small.csv
- **large.csv**: ~1,170x larger than small.csv
- **xlarge.csv**: ~13,100x larger than small.csv

The actual performance difference between tools typically becomes more pronounced with larger files, where cisv's SIMD optimizations and memory-mapped I/O should show greater benefits compared to traditional parsers.

## RUN BENCHMARK YOURSELF (with docker)

```bash
# Build the Docker image
$ docker build -t cisv-benchmark .

# Run with CPU and memory limits for consistent results
# This example uses 2 CPUs and 4GB RAM
$ docker run --rm \
  --cpus="2.0" \
  --memory="4g" \
  --memory-swap="4g" \
  --cpu-shares=1024 \
  --security-opt seccomp=unconfined \
  cisv-benchmark

# For even more isolation, use specific CPU cores (e.g., cores 2-3)
$ docker run --rm \
  --cpuset-cpus="2-3" \
  --memory="4g" \
  --memory-swap="4g" \
  --cpu-shares=1024 \
  --security-opt seccomp=unconfined \
  cisv-benchmark

# To save results to host
$ docker run --rm \
  --cpus="2.0" \
  --memory="4g" \
  --memory-swap="4g" \
  -v $(pwd)/results:/results \
  cisv-benchmark bash -c '/home/benchmark/run_benchmarks.sh | tee /results/benchmark-$(date +%Y%m%d-%H%M%S).log'
```

Or, for even better isolation and consistent results, you can disable CPU frequency scaling on the host

```bash
# On the host system (requires root)
# Save current governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /tmp/governors.txt

# Set performance mode for consistent CPU frequency
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu
done

# Run benchmarks
docker run --rm --cpus="2.0" --memory="4g" cisv-benchmark

# Restore original governors
# ... restore from /tmp/governors.txt
```

## INSTALLATION

### NODE.JS PACKAGE (napi binding)

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

## USAGE

### CLI INTERFACE

#### Reading/Parsing CSV

```console
$ ls -alh ./cisv
Permissions Size User Date Modified Name
.rwxrwxr-x   18k dk   29 Jul  9:34  ./cisv
```

```bash
$ ./cisv --help
cisv - The fastest CSV parser of the multiverse

Usage: ./cisv [COMMAND] [OPTIONS] [FILE]

Commands:
  parse    Parse CSV file (default if no command given)
  write    Write/generate CSV files

Parse Options:
  -h, --help              Show this help message
  -v, --version           Show version information
  -d, --delimiter DELIM   Field delimiter (default: ,)
  -s, --select COLS       Select columns (comma-separated indices)
  -c, --count             Show only row count
  --head N                Show first N rows
  --tail N                Show last N rows
  -o, --output FILE       Write to FILE instead of stdout
  -b, --benchmark         Run benchmark mode

For write options, use: ./cisv write --help
```

```bash
# Count rows (blazing fast)
cisv -c large_file.csv

# Select specific columns
cisv -s 0,2,5 data.csv

# First 100 rows
cisv --head 100 data.csv

# Use semicolon delimiter
cisv -d ';' european_data.csv

# Benchmark mode
cisv -b huge_file.csv

# Output to file
cisv -o processed.csv raw_data.csv
```

#### Writing/Generating CSV

```bash
# Generate 1 million rows of test data
cisv write -g 1000000 -o test_data.csv

# Generate with custom delimiter
cisv write -g 10000 -d '|' -o pipe_delimited.csv

# Always quote all fields
cisv write -g 10000 -Q -o quoted.csv

# Use CRLF line endings (Windows)
cisv write -g 10000 -r -o windows.csv

# Benchmark write performance
cisv write -g 10000000 -o large.csv -b

# Custom null representation
cisv write -g 1000 -n "NULL" -o with_nulls.csv
```

### NODE.JS API

```javascript
const { cisvParser } = require('cisv');

// Synchronous parsing
const parser = new cisvParser();
const rows = parser.parseSync('./data.csv');
console.log(`Parsed ${rows.length} rows`);

// Streaming for large files
const fs = require('fs');
const parser = new cisvParser();

fs.createReadStream('./huge.csv')
  .on('data', chunk => parser.write(chunk))
  .on('end', () => {
    parser.end();
    const rows = parser.getRows();
    console.log(`Processed ${rows.length} rows`);
  });
```

### TYPESCRIPT SUPPORT

(yes)

```typescript
import { cisvParser } from 'cisv';

const parser = new cisvParser();
const rows: string[][] = parser.parseSync('./data.csv');
```

## TECHNICAL DETAILS

### ARCHITECTURE

- **SIMD Processing**: Utilizes AVX-512 (64-byte vectors) or AVX2 (32-byte vectors) for parallel character processing
- **Memory Mapping**: Direct kernel-to-userspace zero-copy with `mmap()`
- **Optimized Buffering**: 1MB ring buffer sized for L3 cache efficiency (still considering upgrading this...)
- **Compiler Optimizations**: LTO and architecture-specific tuning with `-march=native`

### KEY FEATURES

- [x] RFC 4180 compliant
- [x] Handles quoted fields with embedded delimiters
- [x] Streaming API for unlimited file sizes
- [x] Cross-platform (Linux, macOS, Windows via WSL)
- [x] Safe fallback for non-x86 architectures
- [x] High-performance CSV writer with SIMD optimization

## CONTRIBUTING

Contributions are welcome! Areas of interest:

- [ ] ARM NEON/SVE support
- [ ] Windows native support (without WSL)
- [ ] Parallel parsing for multi-core systems
- [ ] Custom memory allocators
- [ ] Streaming compression support

Please ensure all tests pass and benchmarks show no regression.

## LICENSE

GPL2 © [sanix-darker](https://github.com/sanix-darker)

## ACKNOWLEDGMENTS

Inspired by the excellent work in:
- [simdjson](https://github.com/simdjson/simdjson) - Parsing gigabytes of JSON per second
- [xsv](https://github.com/BurntSushi/xsv) - A fast CSV command line toolkit
- [rust-csv](https://github.com/BurntSushi/rust-csv) - A CSV parser for Rust, with Serde support.
