# CISV Python Benchmark

Isolated Docker-based benchmark comparing CISV against popular Python CSV parsers.

## Libraries Compared

| Library | Description |
|---------|-------------|
| **cisv** | High-performance C parser with SIMD optimizations |
| **polars** | Rust-based DataFrame library with Arrow backend |
| **pyarrow** | Apache Arrow's CSV reader (columnar format) |
| **pandas** | Popular DataFrame library |
| **duckdb** | SQL database with fast CSV import |
| **stdlib csv** | Python's built-in csv module |

## Quick Start

### Build the Docker image

```bash
# From repository root
docker build -t sanix-darker/cisv-python-benchmarks -f bindings/python-nanobind/benchmarks/Dockerfile .
```

### Run the benchmark

```bash
# Default: 1M rows x 10 columns with CPU/RAM isolation
docker run -ti --cpus=2 --memory=4g --memory-swap=4g --rm sanix-darker/cisv-python-benchmarks:latest
```

### Custom configurations

```bash
# 10M rows x 20 columns
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-python-benchmarks:latest \
    --rows 10000000 --cols 20

# Fast mode (parallel parsing with numpy arrays)
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-python-benchmarks:latest \
    --rows 1000000 --fast

# Benchmark mode (raw parsing speed, no data marshaling)
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-python-benchmarks:latest \
    --rows 1000000 --benchmark

# Only cisv modes comparison
docker run -ti --cpus=2 --memory=4g --rm sanix-darker/cisv-python-benchmarks:latest \
    --rows 1000000 --only-cisv

# Use an existing CSV file (mount volume)
docker run -ti --cpus=2 --memory=4g --rm \
    -v /path/to/data:/data \
    sanix-darker/cisv-python-benchmarks:latest \
    --file /data/large.csv
```

## CISV Benchmark Modes

| Mode | Description |
|------|-------------|
| `cisv` | Single-threaded, returns `list[list[str]]` |
| `cisv-parallel` | Multi-threaded, returns `list[list[str]]` |
| `cisv-fast` | Multi-threaded + numpy arrays (faster than list output) |
| `cisv-bench` | Multi-threaded, no data marshaling (raw parsing speed) |

## Resource Isolation

The Docker container runs with strict resource limits:
- **CPU**: 2 cores
- **RAM**: 4GB
- **Swap**: Disabled (no disk I/O for memory)

This ensures reproducible benchmarks across different machines.

## Local Development

To run benchmarks locally without Docker:

```bash
# Install dependencies
pip install cisv polars pyarrow pandas duckdb numpy

# Run benchmark
python bindings/python-nanobind/benchmarks/benchmark.py --rows 1000000 --cols 10
```
