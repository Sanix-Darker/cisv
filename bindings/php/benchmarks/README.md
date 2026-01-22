# CISV PHP Benchmark

Isolated Docker-based benchmark comparing CISV PHP extension against native PHP CSV parsing methods.

## Libraries Compared

| Library | Description |
|---------|-------------|
| **cisv (parse)** | High-performance C parser with SIMD optimizations (PHP extension) |
| **cisv (count)** | Fast row counting using C library |
| **fgetcsv()** | PHP's built-in file-based CSV reader |
| **str_getcsv()** | PHP's string-based CSV parser |
| **SplFileObject** | Object-oriented file reader with CSV support |
| **explode** | Simple comma splitting (fast but not RFC compliant) |
| **array_map** | Functional approach with str_getcsv |
| **preg_split** | Regex-based splitting |
| **league/csv** | Popular third-party CSV library (if installed) |

## Quick Start

### Build the Docker image

```bash
# From repository root
docker build -t sanix-darker/cisv-php-benchmarks -f bindings/php/benchmarks/Dockerfile .
```

### Run the benchmark

```bash
# Default: 1M rows x 7 columns with CPU/RAM isolation
docker run -ti --cpus=2 --memory=4g --memory-swap=4g --rm sanix-darker/cisv-php-benchmarks:latest
```

### Custom configurations

```bash
# 500K rows
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-php-benchmarks:latest \
    --rows=500000

# 10 iterations for better accuracy
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-php-benchmarks:latest \
    --rows=1000000 --iterations=10

# Custom column count
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-php-benchmarks:latest \
    --rows=1000000 --cols=20

# Use an existing CSV file (mount volume)
docker run -ti --cpus=2 --memory=4g --rm \
    -v /path/to/data:/data \
    sanix-darker/cisv-php-benchmarks:latest \
    --file=/data/large.csv
```

## Resource Isolation

The Docker container runs with strict resource limits:
- **CPU**: 2 cores
- **RAM**: 4GB
- **Swap**: Disabled (no disk I/O for memory)

This ensures reproducible benchmarks across different machines.

## Local Development

To run benchmarks locally without Docker:

```bash
# Build the PHP extension
cd bindings/php
phpize
./configure
make

# Run benchmark with the extension loaded
php -d extension=modules/cisv.so benchmarks/benchmark.php --rows=100000
```

## Notes

- **cisv (count)** shows raw C library performance without PHP array creation overhead
- **cisv (parse)** includes the cost of converting C data structures to PHP arrays
- **explode** is fast but doesn't handle quoted fields correctly (not RFC 4180 compliant)
- **league/csv** requires Composer installation and is not included by default
