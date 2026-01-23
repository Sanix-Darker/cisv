# CISV CLI Benchmark

Isolated Docker-based benchmark comparing CISV CLI against other popular CSV command-line tools.

## Tools Compared

### High-Performance Tools (Fast Category)

| Tool | Language | Description |
|------|----------|-------------|
| **cisv** | C | High-performance CSV parser with SIMD optimizations |
| **qsv** | Rust | Fork of xsv with 80+ commands, SIMD-accelerated UTF-8 validation |
| **xsv** | Rust | Original fast CSV toolkit by BurntSushi |
| **csvtk** | Go | Cross-platform CSV/TSV toolkit with streaming support |
| **tsv-utils** | D | eBay's tools optimized for large datasets |
| **frawk** | Rust | AWK-like tool with native CSV support and parallel processing |

### Medium-Performance Tools

| Tool | Language | Description |
|------|----------|-------------|
| **miller** | Go | Like awk/sed/cut for CSV/JSON with name-indexed data |
| **datamash** | C | GNU statistical operations on TSV/CSV |
| **goawk** | Go | AWK with native CSV support (--csv flag) |

### Slow Tools (Skip with --fast)

| Tool | Language | Description |
|------|----------|-------------|
| **csvkit** | Python | Full-featured CSV utilities |

### Baseline Unix Tools

| Tool | Description |
|------|-------------|
| **wc** | Line counting |
| **awk** | Text processing |
| **cut** | Column extraction |

## Quick Start

### Build the Docker image

```bash
# From repository root
docker build -t cisv-cli-bench -f cli/benchmarks/Dockerfile .
```

### Run the benchmark

```bash
# Default: 100k rows x 7 columns
docker run --cpus=2 --memory=4g --rm cisv-cli-bench

# Custom row count
docker run --cpus=2 --memory=4g --rm cisv-cli-bench --rows=1000000

# Fast mode (skip slow/medium tools like csvkit, miller, datamash, goawk)
docker run --cpus=2 --memory=4g --rm cisv-cli-bench --rows=100000 --fast

# More iterations
docker run --cpus=2 --memory=4g --rm cisv-cli-bench --rows=100000 --iterations=10
```

### Command-line options

| Option | Default | Description |
|--------|---------|-------------|
| `--rows=N` | 1000000 | Number of rows to generate |
| `--cols=N` | 7 | Number of columns |
| `--iterations=N` | 5 | Benchmark iterations |
| `--file=PATH` | - | Use existing CSV file |
| `--fast` | - | Skip slow/medium tools |

## Tool Categories

The benchmark organizes tools into three performance tiers:

| Category | Tools | When Run |
|----------|-------|----------|
| **Fast** | cisv, qsv, xsv, csvtk, tsv-utils, frawk, wc, awk, cut | Always |
| **Medium** | miller, datamash, goawk | Unless `--fast` |
| **Slow** | csvkit | Unless `--fast` |

## Output Format

The benchmark outputs results in a consistent format matching PHP/Python bindings:

```
============================================================
CISV CLI Benchmark
============================================================

Generating CSV: 100,000 rows × 7 columns...
  Done in 0.42s, file size: 8.6 MB

============================================================
BENCHMARK: 100,000 rows × 7 columns
File size: 8.6 MB
Iterations: 5
Fast mode: false
============================================================

--- Row Counting Benchmarks ---

Benchmarking cisv...
Benchmarking qsv...
Benchmarking xsv...
...

============================================================
RESULTS: count
============================================================
Library          Parse Time     Throughput         Rows
------------------------------------------------------------
cisv                 0.042s     204.8 MB/s      100,001
qsv                  0.048s     179.2 MB/s      100,000
xsv                  0.052s     165.4 MB/s      100,000
csvtk                0.089s      96.6 MB/s      100,000
frawk                0.095s      90.5 MB/s      100,001
...

============================================================
RESULTS: select
============================================================
Library          Parse Time     Throughput         Rows
------------------------------------------------------------
cisv                 0.089s      96.6 MB/s      100,001
qsv                  0.095s      90.5 MB/s      100,001
cut                  0.112s      76.8 MB/s      100,001
...

Benchmark complete!
```

## Benchmark Categories

### Row Counting

Tests how fast each tool can count rows in a CSV file.

| Tool | Command | Notes |
|------|---------|-------|
| cisv | `cisv -c file.csv` | SIMD-optimized |
| qsv | `qsv count file.csv` | Returns row count |
| xsv | `xsv count file.csv` | Returns row count |
| csvtk | `csvtk nrow file.csv` | Streaming |
| tsv-utils | `tsv-summarize --count file.csv` | Optimized for large data |
| frawk | `frawk -i csv 'END{print NR}' file.csv` | Parallel capable |
| miller | `mlr --csv count file.csv` | JSON output |
| datamash | `datamash -H count 1 < file.csv` | Needs header skip |
| goawk | `goawk --csv 'END{print NR}' file.csv` | AWK with CSV |
| wc | `wc -l < file.csv` | Baseline |
| awk | `awk 'END{print NR}' file.csv` | Baseline |
| csvkit | `csvstat --count file.csv` | Python, slow |

### Column Selection

Tests how fast each tool can extract specific columns.

| Tool | Command | Notes |
|------|---------|-------|
| cisv | `cisv -s 0,2,3 file.csv` | 0-indexed |
| qsv | `qsv select 1,3,4 file.csv` | 1-indexed |
| xsv | `xsv select 1,3,4 file.csv` | 1-indexed |
| csvtk | `csvtk cut -f 1,3,4 file.csv` | 1-indexed |
| tsv-utils | `tsv-select -d, -f 1,3,4 file.csv` | 1-indexed, CSV via -d |
| frawk | `frawk -i csv -o csv '{print $1,$3,$4}' file.csv` | 1-indexed |
| miller | `mlr --csv cut -f col0,col2,col3 file.csv` | By name |
| goawk | `goawk --csv '{print $1,$3,$4}' file.csv` | 1-indexed |
| cut | `cut -d',' -f1,3,4 file.csv` | 1-indexed |
| awk | `awk -F',' '{print $1,$3,$4}' file.csv` | 1-indexed |
| csvkit | `csvcut -c 1,3,4 file.csv` | 1-indexed |

## Tool Installation

The Docker image installs all tools automatically. For local installation:

| Tool | Installation |
|------|--------------|
| qsv | `cargo install qsv` or prebuilt binaries |
| xsv | `cargo install xsv` |
| csvtk | Download binaries or `conda install csvtk` |
| tsv-utils | Download binaries from GitHub |
| frawk | `cargo install frawk` |
| miller | `apt install miller` |
| datamash | `apt install datamash` |
| goawk | `go install github.com/benhoyt/goawk@latest` |
| csvkit | `pip install csvkit` |

## Local Development

To run benchmarks locally without Docker:

```bash
# Build cisv
make core cli

# Run benchmark
./cli/benchmarks/run_benchmark.sh --rows=100000 --fast
```

## Resource Isolation

The Docker container should be run with resource limits for reproducible results:

- **CPU**: 2 cores (`--cpus=2`)
- **RAM**: 4GB (`--memory=4g`)

## Verifying Tool Installation

After building the Docker image, verify all tools are installed:

```bash
docker run --rm --entrypoint bash cisv-cli-bench -c "which cisv qsv xsv csvtk frawk mlr goawk datamash"
```
