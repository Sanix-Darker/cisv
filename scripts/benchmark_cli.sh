#!/bin/bash
#
# CISV Benchmark Suite
# Comprehensive benchmarks for CLI, Node.js, Python, and PHP bindings
#

set -uo pipefail

# ============================================================================
# CONFIGURATION
# ============================================================================

ITERATIONS=5
SIZES=("large")
FORCE_REGENERATE=false
NO_CLEANUP=false

# Results storage
declare -A RESULTS

# All logging goes to stderr, only markdown to stdout
log() {
    echo "$@" >&2
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

get_file_size() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        stat -f%z "$1" 2>/dev/null || echo "0"
    else
        stat -c%s "$1" 2>/dev/null || echo "0"
    fi
}

get_file_size_mb() {
    local size=$(get_file_size "$1")
    awk "BEGIN {printf \"%.2f\", ${size:-0} / 1048576}"
}

get_row_count() {
    wc -l < "$1" 2>/dev/null | tr -d ' '
}

# ============================================================================
# INSTALLATION FUNCTIONS
# ============================================================================

install_rust() {
    if ! command_exists cargo; then
        log "Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs 2>/dev/null | sh -s -- -y >/dev/null 2>&1
        source "$HOME/.cargo/env" 2>/dev/null || true
    fi
}

install_miller() {
    if ! command_exists mlr; then
        log "Installing Miller..."
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            if command_exists apt-get; then
                sudo apt-get update -qq >/dev/null 2>&1
                sudo apt-get install -y miller >/dev/null 2>&1 || {
                    local MLR_VERSION="6.12.0"
                    curl -sL "https://github.com/johnkerl/miller/releases/download/v${MLR_VERSION}/miller-${MLR_VERSION}-linux-amd64.tar.gz" 2>/dev/null | tar xz 2>/dev/null
                    sudo mv miller-${MLR_VERSION}-linux-amd64/mlr /usr/local/bin/ 2>/dev/null || true
                    rm -rf miller-${MLR_VERSION}-linux-amd64 2>/dev/null || true
                }
            fi
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            brew install miller >/dev/null 2>&1 || true
        fi
    fi
}

install_csvkit() {
    if command_exists pip3 && ! command_exists csvstat; then
        log "Installing csvkit..."
        pip3 install --quiet csvkit >/dev/null 2>&1 || true
    fi
}

build_rust_csv_tools() {
    log "Building rust-csv benchmark tools..."
    mkdir -p benchmark/rust-csv-bench
    cd benchmark/rust-csv-bench

    cat > Cargo.toml << 'CARGO_EOF'
[package]
name = "csv-bench"
version = "0.1.0"
edition = "2021"

[dependencies]
csv = "1.3"

[[bin]]
name = "csv-bench"
path = "src/main.rs"

[[bin]]
name = "csv-select"
path = "src/select.rs"

[profile.release]
lto = true
codegen-units = 1
CARGO_EOF

    mkdir -p src
    cat > src/main.rs << 'RUST_EOF'
use std::env;
use csv::ReaderBuilder;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 { std::process::exit(1); }
    let mut rdr = ReaderBuilder::new().has_headers(true).from_path(&args[1]).unwrap();
    println!("{}", rdr.records().count());
}
RUST_EOF

    cat > src/select.rs << 'RUST_EOF'
use std::env;
use csv::{ReaderBuilder, WriterBuilder};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 { std::process::exit(1); }
    let columns: Vec<usize> = args[2].split(',').map(|s| s.parse().unwrap()).collect();
    let mut rdr = ReaderBuilder::new().has_headers(true).from_path(&args[1]).unwrap();
    let mut wtr = WriterBuilder::new().from_writer(std::io::stdout());
    let headers = rdr.headers().unwrap().clone();
    let selected: Vec<&str> = columns.iter().map(|&i| headers.get(i).unwrap_or("")).collect();
    wtr.write_record(&selected).unwrap();
    for result in rdr.records() {
        let record = result.unwrap();
        let selected: Vec<&str> = columns.iter().map(|&i| record.get(i).unwrap_or("")).collect();
        wtr.write_record(&selected).unwrap();
    }
}
RUST_EOF

    cargo build --release >/dev/null 2>&1
    cd ../..
}

build_cisv() {
    log "Building cisv..."
    make clean >/dev/null 2>&1 || true
    make all >/dev/null 2>&1
}

install_dependencies() {
    log "Installing benchmark dependencies..."
    install_rust
    build_rust_csv_tools
    install_miller
    install_csvkit
    log "Dependencies installed."
}

# ============================================================================
# BENCHMARK CORE FUNCTION
# ============================================================================

run_benchmark() {
    local cmd="$1"
    local file="$2"
    local extra="${3:-}"

    local total_time=0
    local success_count=0

    for _ in $(seq 1 $ITERATIONS); do
        local start=$(date +%s.%N 2>/dev/null || date +%s)

        local full_cmd="$cmd \"$file\""
        [ -n "$extra" ] && full_cmd="$cmd \"$file\" $extra"

        if timeout 120 bash -c "$full_cmd" >/dev/null 2>&1; then
            local end=$(date +%s.%N 2>/dev/null || date +%s)
            local elapsed=$(awk "BEGIN {print $end - $start}")
            total_time=$(awk "BEGIN {print $total_time + $elapsed}")
            success_count=$((success_count + 1))
        fi
    done

    if [ $success_count -eq 0 ]; then
        echo "FAILED|0"
    else
        local avg=$(awk "BEGIN {printf \"%.4f\", $total_time / $success_count}")
        echo "$avg|$success_count"
    fi
}

# ============================================================================
# TEST DATA GENERATION
# ============================================================================

generate_csv() {
    local rows=$1
    local filename=$2

    python3 << PYEOF
import csv
import random
import string

def rs(n=10):
    return ''.join(random.choices(string.ascii_letters, k=n))

with open('$filename', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range($rows):
        w.writerow([i, f'Person {rs(8)}', f'p{i}@test.com', f'{i} {rs(6)} St', f'555-{i:04d}', f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
PYEOF
}

generate_test_files() {
    if [ ! -f "large.csv" ] || [ "$FORCE_REGENERATE" = "true" ]; then
        log "Generating large.csv (1M rows)..."
        generate_csv 1000000 "large.csv"
    fi
}

# ============================================================================
# CLI BENCHMARKS
# ============================================================================

run_cli_benchmarks() {
    local file="$1"

    export LD_LIBRARY_PATH="${PWD}/core/build:${LD_LIBRARY_PATH:-}"
    local CISV_BIN="${PWD}/cli/build/cisv"
    local RUST_COUNT="./benchmark/rust-csv-bench/target/release/csv-bench"
    local RUST_SELECT="./benchmark/rust-csv-bench/target/release/csv-select"

    log "Running CLI benchmarks..."

    # Row counting
    log "  Row counting tests..."
    [ -x "$CISV_BIN" ] && RESULTS["count_cisv"]=$(run_benchmark "$CISV_BIN -c" "$file")
    [ -x "$RUST_COUNT" ] && RESULTS["count_rust-csv"]=$(run_benchmark "$RUST_COUNT" "$file")
    RESULTS["count_wc_-l"]=$(run_benchmark "wc -l" "$file")
    command_exists csvstat && RESULTS["count_csvkit"]=$(run_benchmark "csvstat --count" "$file")
    command_exists mlr && RESULTS["count_miller"]=$(run_benchmark "mlr --csv count" "$file")

    # Column selection
    log "  Column selection tests..."
    [ -x "$CISV_BIN" ] && RESULTS["select_cisv"]=$(run_benchmark "$CISV_BIN -s 0,2,3" "$file")
    [ -x "$RUST_SELECT" ] && RESULTS["select_rust-csv"]=$(run_benchmark "$RUST_SELECT" "$file" "0,2,3")
    command_exists csvcut && RESULTS["select_csvkit"]=$(run_benchmark "csvcut -c 1,3,4" "$file")
    command_exists mlr && RESULTS["select_miller"]=$(run_benchmark "mlr --csv cut -f id,email,address" "$file")
}

# ============================================================================
# NODE.JS BENCHMARKS
# ============================================================================

run_nodejs_benchmarks() {
    local file="$1"
    local abs_file="${PWD}/${file}"

    if [ ! -f "./bindings/nodejs/package.json" ] || ! command_exists npm; then
        log "Skipping Node.js benchmarks (not available)"
        return
    fi

    log "Running Node.js benchmarks..."

    # Build Node.js binding
    (
        cd ./bindings/nodejs
        npm install >/dev/null 2>&1
        npm run build >/dev/null 2>&1
    )

    # Create benchmark script in the nodejs directory
    cat > ./bindings/nodejs/_bench.js << 'JSEOF'
const fs = require('fs');
const file = process.argv[2];
const parser = process.argv[3];
const iterations = 5;

async function benchmark() {
    let totalTime = 0;
    let success = 0;

    for (let i = 0; i < iterations; i++) {
        const start = process.hrtime.bigint();
        try {
            if (parser === 'cisv') {
                const { cisvParser } = require('./cisv/index.js');
                const p = new cisvParser();
                p.parseSync(file);
            } else if (parser === 'papaparse') {
                const Papa = require('papaparse');
                const content = fs.readFileSync(file, 'utf8');
                Papa.parse(content);
            } else if (parser === 'csv-parse') {
                const { parse } = require('csv-parse/sync');
                const content = fs.readFileSync(file, 'utf8');
                parse(content);
            } else if (parser === 'fast-csv') {
                const { parseFile } = require('fast-csv');
                await new Promise((resolve, reject) => {
                    const rows = [];
                    parseFile(file)
                        .on('data', row => rows.push(row))
                        .on('end', resolve)
                        .on('error', reject);
                });
            }
            const end = process.hrtime.bigint();
            totalTime += Number(end - start) / 1e9;
            success++;
        } catch (e) {}
    }

    if (success === 0) {
        console.log('FAILED|0');
    } else {
        console.log((totalTime / success).toFixed(4) + '|' + success);
    }
}

benchmark();
JSEOF

    # Run benchmarks
    for parser in "cisv" "papaparse" "csv-parse" "fast-csv"; do
        local result
        result=$(cd ./bindings/nodejs && node _bench.js "$abs_file" "$parser" 2>/dev/null) || result="FAILED|0"
        [ -z "$result" ] && result="FAILED|0"
        RESULTS["nodejs_${parser}"]="$result"
        log "    ${parser}: $result"
    done

    rm -f ./bindings/nodejs/_bench.js
}

# ============================================================================
# PYTHON BENCHMARKS
# ============================================================================

run_python_benchmarks() {
    local file="$1"
    local abs_file="${PWD}/${file}"

    if ! command_exists python3; then
        log "Skipping Python benchmarks (python3 not available)"
        return
    fi

    log "Running Python benchmarks..."

    export LD_LIBRARY_PATH="${PWD}/core/build:${LD_LIBRARY_PATH:-}"

    # cisv-python benchmark
    local result
    result=$(python3 << PYEOF 2>/dev/null
import time
import sys
import os

# Add bindings path
sys.path.insert(0, '${PWD}/bindings/python')
os.environ['LD_LIBRARY_PATH'] = '${PWD}/core/build:' + os.environ.get('LD_LIBRARY_PATH', '')

try:
    from cisv import parse_file
    iterations = 5
    total_time = 0
    success = 0

    for _ in range(iterations):
        start = time.perf_counter()
        rows = parse_file('${abs_file}')
        end = time.perf_counter()
        total_time += end - start
        success += 1

    print(f"{total_time/success:.4f}|{success}")
except Exception as e:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_cisv-python"]="$result"
    log "    cisv-python: $result"

    # pandas benchmark
    result=$(python3 << PYEOF 2>/dev/null
import time
try:
    import pandas as pd
    iterations = 5
    total_time = 0

    for _ in range(iterations):
        start = time.perf_counter()
        df = pd.read_csv('${abs_file}')
        end = time.perf_counter()
        total_time += end - start

    print(f"{total_time/iterations:.4f}|{iterations}")
except:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_pandas"]="$result"
    log "    pandas: $result"

    # stdlib csv benchmark
    result=$(python3 << PYEOF 2>/dev/null
import time
import csv

iterations = 5
total_time = 0

for _ in range(iterations):
    start = time.perf_counter()
    with open('${abs_file}', 'r') as f:
        reader = csv.reader(f)
        rows = list(reader)
    end = time.perf_counter()
    total_time += end - start

print(f"{total_time/iterations:.4f}|{iterations}")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_csv-stdlib"]="$result"
    log "    csv-stdlib: $result"
}

# ============================================================================
# PHP BENCHMARKS
# ============================================================================

run_php_benchmarks() {
    local file="$1"
    local abs_file="${PWD}/${file}"

    if ! command_exists php; then
        log "Skipping PHP benchmarks (php not available)"
        return
    fi

    log "Running PHP benchmarks..."

    # PHP native fgetcsv benchmark
    local result
    result=$(php << 'PHPEOF' 2>/dev/null
<?php
$file = $argv[1] ?? getenv('BENCH_FILE');
$iterations = 5;
$totalTime = 0;

for ($i = 0; $i < $iterations; $i++) {
    $start = microtime(true);
    $handle = fopen($file, 'r');
    $rows = [];
    while (($row = fgetcsv($handle)) !== false) {
        $rows[] = $row;
    }
    fclose($handle);
    $end = microtime(true);
    $totalTime += ($end - $start);
}

echo number_format($totalTime / $iterations, 4) . "|" . $iterations;
PHPEOF
) || result="FAILED|0"
    # Run with file argument
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$handle = fopen(\$file, 'r');
    \$rows = [];
    while ((\$row = fgetcsv(\$handle)) !== false) {
        \$rows[] = \$row;
    }
    fclose(\$handle);
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_fgetcsv"]="$result"
    log "    fgetcsv: $result"

    # PHP str_getcsv benchmark (parse string)
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$content = file_get_contents(\$file);
    \$lines = explode(\"\n\", \$content);
    \$rows = [];
    foreach (\$lines as \$line) {
        if (\$line) \$rows[] = str_getcsv(\$line);
    }
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_str_getcsv"]="$result"
    log "    str_getcsv: $result"

    # PHP League CSV benchmark (if available)
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');

// Check if League CSV is available
if (!class_exists('League\Csv\Reader')) {
    echo 'FAILED|0';
    exit;
}

use League\Csv\Reader;

\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$csv = Reader::createFromPath(\$file, 'r');
    \$csv->setHeaderOffset(0);
    \$records = iterator_to_array(\$csv->getRecords());
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_league-csv"]="$result"
    log "    league-csv: $result"
}

# ============================================================================
# MARKDOWN REPORT GENERATION
# ============================================================================

format_result() {
    local key="$1"
    local file_size_mb="$2"
    local result="${RESULTS[$key]:-FAILED|0}"
    local time=$(echo "$result" | cut -d'|' -f1)
    local runs=$(echo "$result" | cut -d'|' -f2)

    if [ "$time" = "FAILED" ] || [ -z "$time" ] || [ "$time" = "0" ]; then
        echo "| - | - | - |"
    else
        local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
        echo "| $time | $speed | $runs/$ITERATIONS |"
    fi
}

generate_markdown_report() {
    local file="$1"
    local file_size_mb=$(get_file_size_mb "$file")
    local row_count=$(get_row_count "$file")

    cat << EOF
# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | $(date -u +"%Y-%m-%d %H:%M:%S UTC") |
| **Commit** | ${GITHUB_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo "local")} |
| **File Size** | ${file_size_mb} MB |
| **Row Count** | ${row_count} |
| **Iterations** | ${ITERATIONS} |
| **Platform** | $(uname -s) $(uname -m) |

---

## Executive Summary

CISV is a high-performance CSV parser written in C with SIMD optimizations (AVX-512, AVX2, SSE2). This benchmark compares CISV against popular CSV parsing tools across different languages and use cases.

---

## 1. CLI Tools Comparison

### 1.1 Row Counting Performance

Task: Count all rows in a ${file_size_mb} MB CSV file with ${row_count} rows.

| Tool | Time (s) | Speed (MB/s) | Runs |
|------|----------|--------------|------|
| **cisv** $(format_result "count_cisv" "$file_size_mb")
| rust-csv $(format_result "count_rust-csv" "$file_size_mb")
| wc -l $(format_result "count_wc_-l" "$file_size_mb")
| csvkit $(format_result "count_csvkit" "$file_size_mb")
| miller $(format_result "count_miller" "$file_size_mb")

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs |
|------|----------|--------------|------|
| **cisv** $(format_result "select_cisv" "$file_size_mb")
| rust-csv $(format_result "select_rust-csv" "$file_size_mb")
| csvkit $(format_result "select_csvkit" "$file_size_mb")
| miller $(format_result "select_miller" "$file_size_mb")

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js bindings.

| Parser | Time (s) | Speed (MB/s) | Runs |
|--------|----------|--------------|------|
| **cisv** $(format_result "nodejs_cisv" "$file_size_mb")
| papaparse $(format_result "nodejs_papaparse" "$file_size_mb")
| csv-parse $(format_result "nodejs_csv-parse" "$file_size_mb")
| fast-csv $(format_result "nodejs_fast-csv" "$file_size_mb")

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python bindings.

| Parser | Time (s) | Speed (MB/s) | Runs |
|--------|----------|--------------|------|
| **cisv** $(format_result "python_cisv-python" "$file_size_mb")
| pandas $(format_result "python_pandas" "$file_size_mb")
| csv (stdlib) $(format_result "python_csv-stdlib" "$file_size_mb")

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP.

| Parser | Time (s) | Speed (MB/s) | Runs |
|--------|----------|--------------|------|
| fgetcsv $(format_result "php_fgetcsv" "$file_size_mb")
| str_getcsv $(format_result "php_str_getcsv" "$file_size_mb")
| league/csv $(format_result "php_league-csv" "$file_size_mb")

---

## 5. Performance Analysis

### Speed Rankings (Row Counting)

EOF

    # Generate rankings
    local rank=1
    local tools=("count_cisv:cisv (C)" "count_rust-csv:rust-csv (Rust)" "count_wc_-l:wc -l (coreutils)" "count_csvkit:csvkit (Python)" "count_miller:miller (Go)")

    # Create temp file for sorting
    local tmpfile=$(mktemp)
    for entry in "${tools[@]}"; do
        local key="${entry%%:*}"
        local name="${entry#*:}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        if [ "$time" != "FAILED" ] && [ -n "$time" ] && [ "$time" != "0" ]; then
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "$speed|$name" >> "$tmpfile"
        fi
    done

    sort -t'|' -k1 -rn "$tmpfile" | while IFS='|' read -r speed name; do
        echo "${rank}. **${name}** - ${speed} MB/s"
        rank=$((rank + 1))
    done
    rm -f "$tmpfile"

    cat << EOF

### Key Observations

- **CISV CLI** uses native C with SIMD optimizations for maximum throughput
- **rust-csv** provides excellent performance with memory safety guarantees
- **wc -l** is fast but only counts lines, not CSV-aware
- **csvkit/miller** trade performance for features and flexibility

### Technology Notes

| Tool | Language | Features |
|------|----------|----------|
| cisv | C | SIMD (AVX-512/AVX2/SSE2), zero-copy parsing |
| rust-csv | Rust | Memory-safe, streaming |
| wc -l | C | Line counting only |
| csvkit | Python | Full CSV toolkit |
| miller | Go | Data transformation |
| papaparse | JavaScript | Browser/Node compatible |
| pandas | Python/C | DataFrame operations |

---

## Methodology

- Each test was run **${ITERATIONS} times** and averaged
- Tests used a **${file_size_mb} MB** CSV file with **${row_count}** rows
- All tests ran on the same machine sequentially
- Warm-up runs were not performed (cold start)
- Times include file I/O and parsing

---

*Report generated by CISV Benchmark Suite*
EOF
}

# ============================================================================
# CLEANUP
# ============================================================================

cleanup() {
    if [ "$NO_CLEANUP" != "true" ]; then
        rm -f small.csv medium.csv large.csv 2>/dev/null
    fi
}

# ============================================================================
# MAIN
# ============================================================================

show_help() {
    cat << EOF >&2
CISV Benchmark Suite

Usage: $0 [MODE] [OPTIONS]

Modes:
    install     Install benchmark dependencies
    benchmark   Run benchmarks and output markdown report

Options:
    --all               Run all benchmarks (default)
    --force-regenerate  Regenerate test CSV files
    --no-cleanup        Keep test files after benchmark
    --help              Show this help

Examples:
    $0 install
    $0 benchmark --all > BENCHMARKS.md
EOF
}

main() {
    local MODE=""

    [ $# -eq 0 ] && { show_help; exit 0; }

    case $1 in
        install) MODE="install"; shift ;;
        benchmark) MODE="benchmark"; shift ;;
        --help|-h) show_help; exit 0 ;;
        *) log "Unknown mode: $1"; show_help; exit 1 ;;
    esac

    while [[ $# -gt 0 ]]; do
        case $1 in
            --all) SIZES=("large"); shift ;;
            --force-regenerate) FORCE_REGENERATE=true; shift ;;
            --no-cleanup) NO_CLEANUP=true; shift ;;
            --help|-h) show_help; exit 0 ;;
            *) shift ;;
        esac
    done

    case $MODE in
        install)
            install_dependencies
            ;;
        benchmark)
            generate_test_files
            build_cisv

            for size in "${SIZES[@]}"; do
                local file="${size}.csv"
                [ ! -f "$file" ] && continue

                run_cli_benchmarks "$file"
                run_nodejs_benchmarks "$file"
                run_python_benchmarks "$file"
                run_php_benchmarks "$file"

                # Output markdown to stdout
                generate_markdown_report "$file"
            done

            cleanup
            log "Benchmark complete!"
            ;;
    esac
}

main "$@"
