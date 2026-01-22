# CISV CLI Benchmark

Isolated Docker-based benchmark comparing CISV CLI against other popular CSV command-line tools.

## Tools Compared

| Tool | Description |
|------|-------------|
| **cisv** | High-performance C CSV parser with SIMD optimizations |
| **xsv** | Rust-based CSV toolkit with parallel processing |
| **wc -l** | Unix line counter (baseline, counts newlines only) |
| **awk** | Text processing tool |
| **cut** | Column extraction tool (delimiter-based) |
| **miller** | Like awk/sed/cut for name-indexed data |

## Quick Start

### Build the Docker image

```bash
# From repository root
docker build -t sanix-darker/cisv-cli-benchmarks -f cli/benchmarks/Dockerfile .
```

### Run the benchmark

```bash
# Default: 1M rows x 7 columns with CPU/RAM isolation
docker run -ti --cpus=2 --memory=4g --memory-swap=4g --rm sanix-darker/cisv-cli-benchmarks:latest
```

### Custom configurations

```bash
# 10M rows
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-cli-benchmarks:latest \
    --rows=10000000

# More iterations for better accuracy
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-cli-benchmarks:latest \
    --rows=1000000 --iterations=10

# Custom column count
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-cli-benchmarks:latest \
    --rows=1000000 --cols=20

# Use an existing CSV file (mount volume)
docker run -ti --cpus=2 --memory=4g --rm \
    -v /path/to/data:/data \
    sanix-darker/cisv-cli-benchmarks:latest \
    --file=/data/large.csv

# Keep test file after benchmark
docker run -ti --cpus=2 --memory=4g --rm \
    -v $(pwd):/output \
    sanix-darker/cisv-cli-benchmarks:latest \
    --no-cleanup
```

## Benchmark Categories

### Row Counting

Tests how fast each tool can count rows in a CSV file.

| Tool | Method |
|------|--------|
| cisv | `cisv -c file.csv` |
| xsv | `xsv count file.csv` |
| wc -l | `wc -l file.csv` |
| awk | `awk 'END{print NR}' file.csv` |
| miller | `mlr --csv count file.csv` |

### Column Selection

Tests how fast each tool can extract specific columns.

| Tool | Method |
|------|--------|
| cisv | `cisv -s 0,2,3 file.csv` |
| xsv | `xsv select 1,3,4 file.csv` |
| awk | `awk -F',' '{print $1,$3,$4}' file.csv` |
| cut | `cut -d',' -f1,3,4 file.csv` |
| miller | `mlr --csv cut -f col0,col2,col3 file.csv` |

## Resource Isolation

The Docker container runs with strict resource limits:
- **CPU**: 2 cores
- **RAM**: 4GB
- **Swap**: Disabled (no disk I/O for memory)

This ensures reproducible benchmarks across different machines.

## Local Development

To run benchmarks locally without Docker:

```bash
# Build cisv
make core cli

# Run benchmark
./cli/benchmarks/run_benchmark.sh --rows=100000
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `ROWS` | 1000000 | Number of rows to generate |
| `COLS` | 7 | Number of columns |
| `ITERATIONS` | 5 | Benchmark iterations |
| `NO_CLEANUP` | false | Keep test files after benchmark |

## Output

The benchmark generates a markdown report on stdout with:
- Test configuration
- Row counting performance table
- Column selection performance table
- Tool descriptions

Logs are printed to stderr for separation.
