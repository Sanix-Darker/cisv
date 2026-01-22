# CISV Node.js Benchmark

Isolated Docker-based benchmark comparing CISV against popular Node.js CSV parsers.

## Libraries Compared

| Library | Description |
|---------|-------------|
| **cisv** | High-performance C parser with SIMD optimizations (N-API binding) |
| **papaparse** | Browser/Node.js CSV parser with streaming support |
| **csv-parse** | Full-featured CSV parser from csv-parser family |
| **fast-csv** | Fast CSV parser with streaming |
| **csv-parser** | Streaming CSV parser |
| **d3-dsv** | D3's delimiter-separated values parser |
| **udsv** | Ultra-fast DSV parser |
| **neat-csv** | Promise-based CSV parser |

## Quick Start

### Build the Docker image

```bash
# From repository root
docker build -t sanix-darker/cisv-nodejs-benchmarks -f bindings/nodejs/benchmarks/Dockerfile .
```

### Run the benchmark

```bash
# Default: 100K rows with CPU/RAM isolation
docker run -ti --cpus=2 --memory=4g --memory-swap=4g --rm sanix-darker/cisv-nodejs-benchmarks:latest
```

### Custom configurations

```bash
# Use a custom CSV file (mount volume)
docker run -ti --cpus=2 --memory=4g --rm \
    -v /path/to/data:/data \
    sanix-darker/cisv-nodejs-benchmarks:latest \
    /data/large.csv
```

## Benchmark Types

The benchmark runs four test suites:

| Suite | Description |
|-------|-------------|
| **Sync (Parse only)** | Synchronous parsing without data access |
| **Sync (Parse + Access)** | Synchronous parsing with row access |
| **Async (Parse only)** | Asynchronous/streaming parsing without data access |
| **Async (Parse + Access)** | Asynchronous/streaming parsing with row access |

## Resource Isolation

The Docker container runs with strict resource limits:
- **CPU**: 2 cores
- **RAM**: 4GB
- **Swap**: Disabled (no disk I/O for memory)

This ensures reproducible benchmarks across different machines.

## Local Development

To run benchmarks locally without Docker:

```bash
cd bindings/nodejs

# Install dependencies
npm install
npm run build
npm install --no-save papaparse csv-parse fast-csv csv-parser d3-dsv udsv neat-csv benchmark

# Run benchmark with a test file
node benchmarks/benchmark.js ../fixtures/data.csv
```

## Output Format

Results are displayed in markdown tables sorted by:
1. Speed (MB/s) - fastest first
2. Operations/sec - most operations first
