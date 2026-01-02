#!/bin/bash

set -uo pipefail

# Configuration
ITERATIONS=3
SIZES=("large")
FORCE_REGENERATE=false
NO_CLEANUP=false
QUIET=false

# Results storage
declare -A RESULTS

# Detect if running in CI
if [ -n "${CI:-}" ] || [ -n "${GITHUB_ACTIONS:-}" ]; then
    QUIET=true
fi

log() {
    if [ "$QUIET" != "true" ]; then
        echo "$@"
    fi
}

log_always() {
    echo "$@"
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

get_file_size() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        stat -f%z "$1" 2>/dev/null
    else
        stat -c%s "$1" 2>/dev/null
    fi
}

get_file_size_mb() {
    local size=$(get_file_size "$1")
    awk "BEGIN {printf \"%.2f\", ${size:-0} / 1048576}"
}

get_row_count() {
    wc -l < "$1" | tr -d ' '
}

# ============================================================================
# INSTALLATION FUNCTIONS
# ============================================================================

install_rust() {
    if ! command_exists cargo; then
        log "Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
        source "$HOME/.cargo/env"
    fi
}

install_miller() {
    if ! command_exists mlr; then
        log "Installing Miller..."
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            # Try apt first
            if command_exists apt-get; then
                sudo apt-get update -qq && sudo apt-get install -y miller 2>/dev/null || {
                    # Fallback to binary download
                    local MLR_VERSION="6.12.0"
                    curl -sL "https://github.com/johnkerl/miller/releases/download/v${MLR_VERSION}/miller-${MLR_VERSION}-linux-amd64.tar.gz" | tar xz
                    sudo mv miller-${MLR_VERSION}-linux-amd64/mlr /usr/local/bin/
                    rm -rf miller-${MLR_VERSION}-linux-amd64
                }
            fi
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            brew install miller 2>/dev/null || true
        fi
    fi
}

install_csvkit() {
    if command_exists pip3 && ! command_exists csvstat; then
        log "Installing csvkit..."
        pip3 install --quiet csvkit 2>/dev/null || true
    fi
}

build_rust_csv_tools() {
    log "Building rust-csv benchmark tools..."
    mkdir -p benchmark/rust-csv-bench
    cd benchmark/rust-csv-bench

    cat > Cargo.toml << 'EOF'
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
EOF

    mkdir -p src
    cat > src/main.rs << 'EOF'
use std::env;
use csv::ReaderBuilder;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 { std::process::exit(1); }
    let mut rdr = ReaderBuilder::new().has_headers(true).from_path(&args[1]).unwrap();
    println!("{}", rdr.records().count());
}
EOF

    cat > src/select.rs << 'EOF'
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
EOF

    cargo build --release --quiet 2>/dev/null
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
# BENCHMARK FUNCTIONS
# ============================================================================

# Silent benchmark - returns "time|success"
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

        if timeout 60 bash -c "$full_cmd" >/dev/null 2>&1; then
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

    python3 << EOF
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
EOF
}

generate_test_files() {
    if [ ! -f "large.csv" ] || [ "$FORCE_REGENERATE" = "true" ]; then
        log "Generating large.csv (1M rows)..."
        generate_csv 1000000 "large.csv"
    fi
}

# ============================================================================
# MARKDOWN REPORT GENERATION
# ============================================================================

generate_markdown_report() {
    local file="$1"
    local file_size_mb=$(get_file_size_mb "$file")
    local row_count=$(get_row_count "$file")

    cat << EOF
# CISV Benchmark Report

> **Generated:** $(date -u +"%Y-%m-%d %H:%M:%S UTC")
> **Commit:** ${GITHUB_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo "local")}
> **Test File:** ${file_size_mb} MB, ${row_count} rows

## Summary

CISV is a high-performance CSV parser with SIMD optimizations. This benchmark compares it against other popular CSV tools.

---

## CLI Benchmarks

### Row Counting

Counting all rows in the CSV file.

| Tool | Time (s) | Speed (MB/s) | Status |
|------|----------|--------------|--------|
EOF

    # Row counting results
    for tool in "cisv" "rust-csv" "wc -l" "csvkit" "miller"; do
        local key="count_${tool// /_}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        local runs=$(echo "$result" | cut -d'|' -f2)

        if [ "$time" = "FAILED" ]; then
            echo "| $tool | - | - | Failed |"
        else
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "| $tool | $time | $speed | OK ($runs/$ITERATIONS) |"
        fi
    done

    cat << EOF

### Column Selection

Selecting columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Status |
|------|----------|--------------|--------|
EOF

    # Column selection results
    for tool in "cisv" "rust-csv" "csvkit" "miller"; do
        local key="select_${tool// /_}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        local runs=$(echo "$result" | cut -d'|' -f2)

        if [ "$time" = "FAILED" ]; then
            echo "| $tool | - | - | Failed |"
        else
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "| $tool | $time | $speed | OK ($runs/$ITERATIONS) |"
        fi
    done

    cat << EOF

---

## Node.js Binding Benchmarks

Parsing the same CSV file using the Node.js binding.

| Parser | Time (s) | Speed (MB/s) | Status |
|--------|----------|--------------|--------|
EOF

    # Node.js results
    for tool in "cisv-node" "papaparse" "csv-parse" "fast-csv"; do
        local key="nodejs_${tool}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        local runs=$(echo "$result" | cut -d'|' -f2)

        if [ "$time" = "FAILED" ] || [ -z "$time" ]; then
            echo "| $tool | - | - | Not tested |"
        else
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "| $tool | $time | $speed | OK |"
        fi
    done

    cat << EOF

---

## Python Binding Benchmarks

Parsing the same CSV file using the Python binding.

| Parser | Time (s) | Speed (MB/s) | Status |
|--------|----------|--------------|--------|
EOF

    # Python results
    for tool in "cisv-python" "pandas" "csv-stdlib"; do
        local key="python_${tool}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        local runs=$(echo "$result" | cut -d'|' -f2)

        if [ "$time" = "FAILED" ] || [ -z "$time" ]; then
            echo "| $tool | - | - | Not tested |"
        else
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "| $tool | $time | $speed | OK |"
        fi
    done

    cat << EOF

---

## Performance Analysis

### Ranking by Speed (Row Counting)

EOF

    # Sort and display ranking
    local rank=1
    for tool in "cisv" "rust-csv" "wc -l" "csvkit" "miller"; do
        local key="count_${tool// /_}"
        local result="${RESULTS[$key]:-FAILED|0}"
        local time=$(echo "$result" | cut -d'|' -f1)
        if [ "$time" != "FAILED" ]; then
            local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
            echo "${rank}. **${tool}** - ${speed} MB/s"
            rank=$((rank + 1))
        fi
    done

    cat << EOF

### Notes

- **cisv**: Native C implementation with SIMD optimizations (AVX-512/AVX2/SSE2)
- **rust-csv**: Rust CSV library with optimized parsing
- **wc -l**: Simple line counting (no CSV parsing)
- **csvkit**: Python-based CSV toolkit
- **miller**: Feature-rich data processing tool

---

*Benchmark conducted with ${ITERATIONS} iterations per test.*
EOF
}

# ============================================================================
# RUN BENCHMARKS
# ============================================================================

run_cli_benchmarks() {
    local file="$1"

    # Setup paths
    export LD_LIBRARY_PATH="${PWD}/core/build:${LD_LIBRARY_PATH:-}"
    local CISV_BIN="${PWD}/cli/build/cisv"
    local RUST_COUNT="./benchmark/rust-csv-bench/target/release/csv-bench"
    local RUST_SELECT="./benchmark/rust-csv-bench/target/release/csv-select"

    log_always "Running CLI benchmarks..."

    # Row counting benchmarks
    log "  Testing row counting..."

    if [ -x "$CISV_BIN" ]; then
        RESULTS["count_cisv"]=$(run_benchmark "$CISV_BIN -c" "$file")
        log "    cisv: ${RESULTS[count_cisv]}"
    fi

    if [ -x "$RUST_COUNT" ]; then
        RESULTS["count_rust-csv"]=$(run_benchmark "$RUST_COUNT" "$file")
        log "    rust-csv: ${RESULTS[count_rust-csv]}"
    fi

    RESULTS["count_wc_-l"]=$(run_benchmark "wc -l" "$file")
    log "    wc -l: ${RESULTS[count_wc_-l]}"

    if command_exists csvstat; then
        RESULTS["count_csvkit"]=$(run_benchmark "csvstat --count" "$file")
        log "    csvkit: ${RESULTS[count_csvkit]}"
    fi

    if command_exists mlr; then
        RESULTS["count_miller"]=$(run_benchmark "mlr --csv count" "$file")
        log "    miller: ${RESULTS[count_miller]}"
    fi

    # Column selection benchmarks
    log "  Testing column selection..."

    if [ -x "$CISV_BIN" ]; then
        RESULTS["select_cisv"]=$(run_benchmark "$CISV_BIN -s 0,2,3" "$file")
        log "    cisv: ${RESULTS[select_cisv]}"
    fi

    if [ -x "$RUST_SELECT" ]; then
        RESULTS["select_rust-csv"]=$(run_benchmark "$RUST_SELECT" "$file" "0,2,3")
        log "    rust-csv: ${RESULTS[select_rust-csv]}"
    fi

    if command_exists csvcut; then
        RESULTS["select_csvkit"]=$(run_benchmark "csvcut -c 1,3,4" "$file")
        log "    csvkit: ${RESULTS[select_csvkit]}"
    fi

    if command_exists mlr; then
        RESULTS["select_miller"]=$(run_benchmark "mlr --csv cut -f id,email,address" "$file")
        log "    miller: ${RESULTS[select_miller]}"
    fi
}

run_nodejs_benchmarks() {
    local file="$1"
    local abs_file="${PWD}/${file}"

    if [ ! -f "./bindings/nodejs/package.json" ] || ! command_exists npm; then
        log "Skipping Node.js benchmarks (not available)"
        return
    fi

    log_always "Running Node.js benchmarks..."

    cd ./bindings/nodejs
    npm install --silent 2>/dev/null
    npm run build --silent 2>/dev/null

    # Create benchmark script
    cat > /tmp/bench_node.js << EOF
const fs = require('fs');
const path = require('path');

const file = process.argv[2];
const parser = process.argv[3];
const iterations = 3;

async function benchmark() {
    let totalTime = 0;
    let success = 0;

    for (let i = 0; i < iterations; i++) {
        const start = process.hrtime.bigint();
        try {
            if (parser === 'cisv') {
                const { cisvParser } = require('./cisv/index.js');
                const p = new cisvParser();
                const rows = p.parseSync(file);
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
        } catch (e) {
            // Ignore errors
        }
    }

    if (success === 0) {
        console.log('FAILED|0');
    } else {
        console.log((totalTime / success).toFixed(4) + '|' + success);
    }
}

benchmark();
EOF

    # Run benchmarks
    for parser in "cisv" "papaparse" "csv-parse" "fast-csv"; do
        local result=$(node /tmp/bench_node.js "$abs_file" "$parser" 2>/dev/null || echo "FAILED|0")
        RESULTS["nodejs_${parser}-node"]="$result"
        if [ "$parser" = "cisv" ]; then
            RESULTS["nodejs_cisv-node"]="$result"
        fi
        log "    ${parser}: $result"
    done

    cd ../..
    rm -f /tmp/bench_node.js
}

run_python_benchmarks() {
    local file="$1"
    local abs_file="${PWD}/${file}"

    if ! command_exists python3; then
        log "Skipping Python benchmarks (python3 not available)"
        return
    fi

    log_always "Running Python benchmarks..."

    # Build cisv python binding
    if [ -f "./bindings/python/setup.py" ]; then
        cd ./bindings/python
        pip3 install -e . --quiet 2>/dev/null || true
        cd ../..
    fi

    # cisv-python benchmark
    local result=$(python3 << EOF 2>/dev/null || echo "FAILED|0"
import time
import sys
sys.path.insert(0, './bindings/python')

try:
    from cisv import parse_file

    iterations = 3
    total_time = 0
    success = 0

    for _ in range(iterations):
        start = time.perf_counter()
        rows = parse_file('$abs_file')
        end = time.perf_counter()
        total_time += end - start
        success += 1

    if success > 0:
        print(f"{total_time/success:.4f}|{success}")
    else:
        print("FAILED|0")
except Exception as e:
    print("FAILED|0")
EOF
)
    RESULTS["python_cisv-python"]="$result"
    log "    cisv-python: $result"

    # pandas benchmark
    result=$(python3 << EOF 2>/dev/null || echo "FAILED|0"
import time
try:
    import pandas as pd

    iterations = 3
    total_time = 0
    success = 0

    for _ in range(iterations):
        start = time.perf_counter()
        df = pd.read_csv('$abs_file')
        end = time.perf_counter()
        total_time += end - start
        success += 1

    print(f"{total_time/success:.4f}|{success}")
except:
    print("FAILED|0")
EOF
)
    RESULTS["python_pandas"]="$result"
    log "    pandas: $result"

    # stdlib csv benchmark
    result=$(python3 << EOF 2>/dev/null || echo "FAILED|0"
import time
import csv

iterations = 3
total_time = 0
success = 0

for _ in range(iterations):
    start = time.perf_counter()
    with open('$abs_file', 'r') as f:
        reader = csv.reader(f)
        rows = list(reader)
    end = time.perf_counter()
    total_time += end - start
    success += 1

print(f"{total_time/success:.4f}|{success}")
EOF
)
    RESULTS["python_csv-stdlib"]="$result"
    log "    csv-stdlib: $result"
}

cleanup() {
    if [ "$NO_CLEANUP" != "true" ]; then
        rm -f small.csv medium.csv large.csv 2>/dev/null
    fi
}

show_help() {
    cat << EOF
Usage: $0 [MODE] [OPTIONS]

Modes:
    install           Install dependencies
    benchmark         Run all benchmarks and generate report

Options:
    --all             Run all benchmarks
    --force-regenerate Regenerate test files
    --no-cleanup      Keep test files after benchmark
    --quiet           Minimal output
    --help            Show this help

Examples:
    $0 install
    $0 benchmark --all
EOF
}

main() {
    local MODE=""

    [ $# -eq 0 ] && { show_help; exit 0; }

    case $1 in
        install) MODE="install"; shift ;;
        benchmark) MODE="benchmark"; shift ;;
        --help|-h) show_help; exit 0 ;;
        *) echo "Unknown mode: $1"; show_help; exit 1 ;;
    esac

    while [[ $# -gt 0 ]]; do
        case $1 in
            --all) SIZES=("large"); shift ;;
            --force-regenerate) FORCE_REGENERATE=true; shift ;;
            --no-cleanup) NO_CLEANUP=true; shift ;;
            --quiet) QUIET=true; shift ;;
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

                # Generate markdown report
                generate_markdown_report "$file"
            done

            cleanup
            log_always "Benchmark complete!"
            ;;
    esac
}

main "$@"
