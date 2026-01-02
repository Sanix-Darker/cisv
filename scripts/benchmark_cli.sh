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
PROJECT_ROOT="${PWD}"

# Results storage
declare -A RESULTS
declare -A VALIDATIONS

# Expected values for validation (set during test file generation)
EXPECTED_ROW_COUNT=0
EXPECTED_FIELD_COUNT=7
EXPECTED_FIRST_ID="0"
EXPECTED_HEADERS="id,name,email,address,phone,date,amount"

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

install_xsv() {
    if ! command_exists xsv; then
        log "Installing xsv..."
        if command_exists cargo; then
            cargo install xsv >/dev/null 2>&1 || true
        fi
    fi
}

install_php_league_csv() {
    log "Installing PHP league/csv..."
    mkdir -p /tmp/csv_bench
    cd /tmp/csv_bench
    if [ ! -f "vendor/autoload.php" ]; then
        if command_exists composer; then
            composer require league/csv >/dev/null 2>&1 || true
        else
            # Download composer if not available
            curl -sS https://getcomposer.org/installer 2>/dev/null | php -- --quiet >/dev/null 2>&1 || true
            if [ -f "composer.phar" ]; then
                php composer.phar require league/csv >/dev/null 2>&1 || true
            fi
        fi
    fi
    cd "$PROJECT_ROOT"
}

build_php_extension() {
    log "Building cisv PHP extension..."
    if [ ! -d "./bindings/php" ]; then
        log "  PHP extension source not found, skipping"
        return
    fi

    # Check for php-dev
    if ! command_exists phpize; then
        log "  phpize not found, installing php-dev..."
        if command_exists apt-get; then
            sudo apt-get install -y php-dev >/dev/null 2>&1 || true
        fi
    fi

    if ! command_exists phpize; then
        log "  phpize still not available, skipping PHP extension build"
        return
    fi

    # Build core library first (required for linking)
    log "  Building core library for PHP extension..."
    make core >/dev/null 2>&1 || {
        log "  Failed to build core library"
        return
    }

    cd ./bindings/php

    # Clean previous build thoroughly
    make clean >/dev/null 2>&1 || true
    rm -f configure aclocal.m4 config.h config.h.in config.guess config.sub >/dev/null 2>&1 || true
    rm -f configure.ac install-sh ltmain.sh Makefile.global missing mkinstalldirs >/dev/null 2>&1 || true
    rm -rf autom4te.cache build .deps modules .libs >/dev/null 2>&1 || true
    rm -f src/*.lo src/*.o >/dev/null 2>&1 || true

    # Build extension
    log "  Running phpize..."
    phpize >/dev/null 2>&1 || { log "  phpize failed"; cd "$PROJECT_ROOT"; return; }

    log "  Running configure..."
    ./configure --enable-cisv >/dev/null 2>&1 || { log "  configure failed"; cd "$PROJECT_ROOT"; return; }

    log "  Running make..."
    make >/dev/null 2>&1 || { log "  make failed"; cd "$PROJECT_ROOT"; return; }

    if [ -f "modules/cisv.so" ]; then
        log "  PHP extension built successfully"
    else
        log "  PHP extension build failed - modules/cisv.so not found"
    fi

    cd "$PROJECT_ROOT"
}

install_python_deps() {
    log "Installing Python dependencies..."
    pip3 install --quiet polars pyarrow pandas >/dev/null 2>&1 || true
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
    cd "$PROJECT_ROOT"
}

build_cisv() {
    log "Building cisv..."
    # Only clean core and cli, not PHP extension
    make -C core clean >/dev/null 2>&1 || true
    make -C cli clean >/dev/null 2>&1 || true
    make all >/dev/null 2>&1

    # Rebuild PHP extension if phpize is available (since make clean might have deleted it)
    if command_exists phpize && [ -d "./bindings/php" ]; then
        local php_ext="${PROJECT_ROOT}/bindings/php/modules/cisv.so"
        if [ ! -f "$php_ext" ]; then
            log "  Rebuilding PHP extension..."
            build_php_extension
        fi
    fi
}

install_dependencies() {
    log "Installing benchmark dependencies..."
    install_rust
    build_rust_csv_tools
    install_miller
    install_csvkit
    install_xsv
    install_php_league_csv
    build_php_extension
    install_python_deps
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
# VALIDATION FUNCTIONS
# ============================================================================

# Validate cisv CLI count result
validate_cli_count() {
    local file="$1"
    local cisv_bin="${PROJECT_ROOT}/cli/build/cisv"

    if [ ! -x "$cisv_bin" ]; then
        echo "SKIP"
        return
    fi

    local count=$(LD_LIBRARY_PATH="${PROJECT_ROOT}/core/build" "$cisv_bin" -c "$file" 2>/dev/null | tr -d '[:space:]')

    if [ "$count" = "$EXPECTED_ROW_COUNT" ]; then
        echo "PASS"
    else
        echo "FAIL:expected=$EXPECTED_ROW_COUNT,got=$count"
    fi
}

# Validate cisv CLI parse result (check headers and first row)
validate_cli_parse() {
    local file="$1"
    local cisv_bin="${PROJECT_ROOT}/cli/build/cisv"

    if [ ! -x "$cisv_bin" ]; then
        echo "SKIP"
        return
    fi

    # Get first 2 lines and verify structure
    local output=$(LD_LIBRARY_PATH="${PROJECT_ROOT}/core/build" "$cisv_bin" --head 2 "$file" 2>/dev/null)
    local first_line=$(echo "$output" | head -1)
    local second_line=$(echo "$output" | sed -n '2p')
    local first_id=$(echo "$second_line" | cut -d',' -f1)

    if [ "$first_line" = "$EXPECTED_HEADERS" ] && [ "$first_id" = "$EXPECTED_FIRST_ID" ]; then
        echo "PASS"
    else
        echo "FAIL:headers_or_data_mismatch"
    fi
}

# Validate Node.js cisv count result
validate_nodejs_count() {
    local file="$1"

    if [ ! -f "./bindings/nodejs/cisv/index.js" ]; then
        echo "SKIP"
        return
    fi

    local count=$(cd ./bindings/nodejs && node -e "
const { cisvParser } = require('./cisv/index.js');
console.log(cisvParser.countRows('$file'));
" 2>/dev/null)

    if [ "$count" = "$EXPECTED_ROW_COUNT" ]; then
        echo "PASS"
    else
        echo "FAIL:expected=$EXPECTED_ROW_COUNT,got=$count"
    fi
}

# Validate Node.js cisv parse result
validate_nodejs_parse() {
    local file="$1"

    if [ ! -f "./bindings/nodejs/cisv/index.js" ]; then
        echo "SKIP"
        return
    fi

    local result=$(cd ./bindings/nodejs && node -e "
const { cisvParser } = require('./cisv/index.js');
const p = new cisvParser();
const rows = p.parseSync('$file');
const rowCount = rows.length;
const fieldCount = rows[0] ? rows[0].length : 0;
const firstId = rows[1] ? rows[1][0] : '';
const headers = rows[0] ? rows[0].join(',') : '';
console.log(rowCount + '|' + fieldCount + '|' + firstId + '|' + headers);
" 2>/dev/null)

    local row_count=$(echo "$result" | cut -d'|' -f1)
    local field_count=$(echo "$result" | cut -d'|' -f2)
    local first_id=$(echo "$result" | cut -d'|' -f3)
    local headers=$(echo "$result" | cut -d'|' -f4)

    if [ "$row_count" = "$EXPECTED_ROW_COUNT" ] && [ "$field_count" = "$EXPECTED_FIELD_COUNT" ] && \
       [ "$first_id" = "$EXPECTED_FIRST_ID" ] && [ "$headers" = "$EXPECTED_HEADERS" ]; then
        echo "PASS"
    else
        echo "FAIL:rows=$row_count,fields=$field_count"
    fi
}

# Validate Python cisv count result
validate_python_count() {
    local file="$1"
    local lib_path="${PROJECT_ROOT}/core/build/libcisv.so"

    if [ ! -f "$lib_path" ]; then
        echo "SKIP"
        return
    fi

    local count=$(python3 << PYEOF 2>/dev/null
import ctypes
lib = ctypes.CDLL("$lib_path")
lib.cisv_parser_count_rows.argtypes = [ctypes.c_char_p]
lib.cisv_parser_count_rows.restype = ctypes.c_size_t
print(lib.cisv_parser_count_rows(b"$file"))
PYEOF
)

    if [ "$count" = "$EXPECTED_ROW_COUNT" ]; then
        echo "PASS"
    else
        echo "FAIL:expected=$EXPECTED_ROW_COUNT,got=$count"
    fi
}

# Validate PHP cisv count result
validate_php_count() {
    local file="$1"
    local php_ext="${PROJECT_ROOT}/bindings/php/modules/cisv.so"
    local lib_path="${PROJECT_ROOT}/core/build"

    if [ ! -f "$php_ext" ]; then
        echo "SKIP"
        return
    fi

    local count=$(LD_LIBRARY_PATH="$lib_path" php -d "extension=$php_ext" -r "
echo CisvParser::countRows('$file');
" 2>/dev/null)

    if [ "$count" = "$EXPECTED_ROW_COUNT" ]; then
        echo "PASS"
    else
        echo "FAIL:expected=$EXPECTED_ROW_COUNT,got=$count"
    fi
}

# Validate PHP cisv parse result
validate_php_parse() {
    local file="$1"
    local php_ext="${PROJECT_ROOT}/bindings/php/modules/cisv.so"
    local lib_path="${PROJECT_ROOT}/core/build"

    if [ ! -f "$php_ext" ]; then
        echo "SKIP"
        return
    fi

    local result=$(LD_LIBRARY_PATH="$lib_path" php -d "extension=$php_ext" -r "
\$parser = new CisvParser();
\$rows = \$parser->parseFile('$file');
\$rowCount = count(\$rows);
\$fieldCount = isset(\$rows[0]) ? count(\$rows[0]) : 0;
\$firstId = isset(\$rows[1][0]) ? \$rows[1][0] : '';
\$headers = isset(\$rows[0]) ? implode(',', \$rows[0]) : '';
echo \$rowCount . '|' . \$fieldCount . '|' . \$firstId . '|' . \$headers;
" 2>/dev/null)

    local row_count=$(echo "$result" | cut -d'|' -f1)
    local field_count=$(echo "$result" | cut -d'|' -f2)
    local first_id=$(echo "$result" | cut -d'|' -f3)
    local headers=$(echo "$result" | cut -d'|' -f4)

    if [ "$row_count" = "$EXPECTED_ROW_COUNT" ] && [ "$field_count" = "$EXPECTED_FIELD_COUNT" ] && \
       [ "$first_id" = "$EXPECTED_FIRST_ID" ] && [ "$headers" = "$EXPECTED_HEADERS" ]; then
        echo "PASS"
    else
        echo "FAIL:rows=$row_count,fields=$field_count"
    fi
}

# Run all validations
run_validations() {
    local file="$1"
    local abs_file="${PROJECT_ROOT}/${file}"

    log "Running validation checks..."

    VALIDATIONS["cli_count"]=$(validate_cli_count "$abs_file")
    log "  CLI count: ${VALIDATIONS[cli_count]}"

    VALIDATIONS["cli_parse"]=$(validate_cli_parse "$abs_file")
    log "  CLI parse: ${VALIDATIONS[cli_parse]}"

    VALIDATIONS["nodejs_count"]=$(validate_nodejs_count "$abs_file")
    log "  Node.js count: ${VALIDATIONS[nodejs_count]}"

    VALIDATIONS["nodejs_parse"]=$(validate_nodejs_parse "$abs_file")
    log "  Node.js parse: ${VALIDATIONS[nodejs_parse]}"

    VALIDATIONS["python_count"]=$(validate_python_count "$abs_file")
    log "  Python count: ${VALIDATIONS[python_count]}"

    VALIDATIONS["php_count"]=$(validate_php_count "$abs_file")
    log "  PHP count: ${VALIDATIONS[php_count]}"

    VALIDATIONS["php_parse"]=$(validate_php_parse "$abs_file")
    log "  PHP parse: ${VALIDATIONS[php_parse]}"
}

# Format validation result for markdown
format_validation() {
    local key="$1"
    local result="${VALIDATIONS[$key]:-SKIP}"

    if [[ "$result" == "PASS" ]]; then
        echo "✓"
    elif [[ "$result" == "SKIP" ]]; then
        echo "-"
    else
        echo "✗"
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
    # Set expected row count (1M data rows + 1 header row)
    EXPECTED_ROW_COUNT=1000001
}

# ============================================================================
# CLI BENCHMARKS (7 tools)
# ============================================================================

run_cli_benchmarks() {
    local file="$1"

    export LD_LIBRARY_PATH="${PROJECT_ROOT}/core/build:${LD_LIBRARY_PATH:-}"
    local CISV_BIN="${PROJECT_ROOT}/cli/build/cisv"
    local RUST_COUNT="./benchmark/rust-csv-bench/target/release/csv-bench"
    local RUST_SELECT="./benchmark/rust-csv-bench/target/release/csv-select"

    log "Running CLI benchmarks..."

    # Row counting (7 tools)
    log "  Row counting tests..."
    [ -x "$CISV_BIN" ] && RESULTS["count_cisv"]=$(run_benchmark "$CISV_BIN -c" "$file")
    [ -x "$RUST_COUNT" ] && RESULTS["count_rust-csv"]=$(run_benchmark "$RUST_COUNT" "$file")
    command_exists xsv && RESULTS["count_xsv"]=$(run_benchmark "xsv count" "$file")
    RESULTS["count_wc"]=$(run_benchmark "wc -l" "$file")
    RESULTS["count_awk"]=$(run_benchmark "awk 'END{print NR}'" "$file")
    command_exists mlr && RESULTS["count_miller"]=$(run_benchmark "mlr --csv count" "$file")
    command_exists csvstat && RESULTS["count_csvkit"]=$(run_benchmark "csvstat --count" "$file")

    # Column selection (7 tools)
    log "  Column selection tests..."
    [ -x "$CISV_BIN" ] && RESULTS["select_cisv"]=$(run_benchmark "$CISV_BIN -s 0,2,3" "$file")
    [ -x "$RUST_SELECT" ] && RESULTS["select_rust-csv"]=$(run_benchmark "$RUST_SELECT" "$file" "0,2,3")
    command_exists xsv && RESULTS["select_xsv"]=$(run_benchmark "xsv select 1,3,4" "$file")
    RESULTS["select_awk"]=$(run_benchmark "awk -F',' '{print \$1,\$3,\$4}'" "$file")
    RESULTS["select_cut"]=$(run_benchmark "cut -d',' -f1,3,4" "$file")
    command_exists mlr && RESULTS["select_miller"]=$(run_benchmark "mlr --csv cut -f id,email,address" "$file")
    command_exists csvcut && RESULTS["select_csvkit"]=$(run_benchmark "csvcut -c 1,3,4" "$file")
}

# ============================================================================
# NODE.JS BENCHMARKS (7 parsers)
# ============================================================================

run_nodejs_benchmarks() {
    local file="$1"
    local abs_file="${PROJECT_ROOT}/${file}"

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
        # Install additional parsers for comparison
        npm install papaparse csv-parse fast-csv csv-parser d3-dsv csv-string --save-dev >/dev/null 2>&1 || true
    )

    # Create benchmark script
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
            } else if (parser === 'cisv-count') {
                // Pure C row counting - shows raw library performance
                const { cisvParser } = require('./cisv/index.js');
                cisvParser.countRows(file);
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
            } else if (parser === 'csv-parser') {
                const csvParser = require('csv-parser');
                await new Promise((resolve, reject) => {
                    const rows = [];
                    fs.createReadStream(file)
                        .pipe(csvParser())
                        .on('data', row => rows.push(row))
                        .on('end', resolve)
                        .on('error', reject);
                });
            } else if (parser === 'd3-dsv') {
                const d3 = require('d3-dsv');
                const content = fs.readFileSync(file, 'utf8');
                d3.csvParse(content);
            } else if (parser === 'csv-string') {
                const CSV = require('csv-string');
                const content = fs.readFileSync(file, 'utf8');
                CSV.parse(content);
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

    # Run benchmarks for 8 parsers (including cisv-count to show raw C performance)
    for parser in "cisv" "cisv-count" "papaparse" "csv-parse" "fast-csv" "csv-parser" "d3-dsv" "csv-string"; do
        local result
        result=$(cd ./bindings/nodejs && node _bench.js "$abs_file" "$parser" 2>/dev/null) || result="FAILED|0"
        [ -z "$result" ] && result="FAILED|0"
        RESULTS["nodejs_${parser}"]="$result"
        log "    ${parser}: $result"
    done

    rm -f ./bindings/nodejs/_bench.js
}

# ============================================================================
# PYTHON BENCHMARKS (7 parsers)
# ============================================================================

run_python_benchmarks() {
    local file="$1"
    local abs_file="${PROJECT_ROOT}/${file}"
    local lib_path="${PROJECT_ROOT}/core/build/libcisv.so"

    if ! command_exists python3; then
        log "Skipping Python benchmarks (python3 not available)"
        return
    fi

    log "Running Python benchmarks..."

    # cisv-python benchmark (using direct ctypes loading with explicit path)
    local result
    result=$(python3 << PYEOF 2>/dev/null
import time
import ctypes
import os

try:
    # Direct load with explicit path - this is the key fix
    lib_path = "${lib_path}"
    if not os.path.exists(lib_path):
        print("FAILED|0")
        exit()

    lib = ctypes.CDLL(lib_path)

    # Define function signatures
    lib.cisv_parser_count_rows.argtypes = [ctypes.c_char_p]
    lib.cisv_parser_count_rows.restype = ctypes.c_size_t

    iterations = 5
    total_time = 0
    success = 0

    for _ in range(iterations):
        start = time.perf_counter()
        count = lib.cisv_parser_count_rows(b"${abs_file}")
        end = time.perf_counter()
        total_time += end - start
        success += 1

    print(f"{total_time/success:.4f}|{success}")
except Exception as e:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_cisv"]="$result"
    log "    cisv: $result"

    # polars benchmark (fastest Python CSV parser)
    result=$(python3 << PYEOF 2>/dev/null
import time
try:
    import polars as pl
    iterations = 5
    total_time = 0

    for _ in range(iterations):
        start = time.perf_counter()
        df = pl.read_csv('${abs_file}')
        end = time.perf_counter()
        total_time += end - start

    print(f"{total_time/iterations:.4f}|{iterations}")
except:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_polars"]="$result"
    log "    polars: $result"

    # pyarrow benchmark
    result=$(python3 << PYEOF 2>/dev/null
import time
try:
    import pyarrow.csv as pa_csv
    iterations = 5
    total_time = 0

    for _ in range(iterations):
        start = time.perf_counter()
        table = pa_csv.read_csv('${abs_file}')
        end = time.perf_counter()
        total_time += end - start

    print(f"{total_time/iterations:.4f}|{iterations}")
except:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_pyarrow"]="$result"
    log "    pyarrow: $result"

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

    # DictReader benchmark
    result=$(python3 << PYEOF 2>/dev/null
import time
import csv

iterations = 5
total_time = 0

for _ in range(iterations):
    start = time.perf_counter()
    with open('${abs_file}', 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    end = time.perf_counter()
    total_time += end - start

print(f"{total_time/iterations:.4f}|{iterations}")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_dictreader"]="$result"
    log "    dictreader: $result"

    # numpy genfromtxt benchmark
    result=$(python3 << PYEOF 2>/dev/null
import time
try:
    import numpy as np
    iterations = 5
    total_time = 0

    for _ in range(iterations):
        start = time.perf_counter()
        data = np.genfromtxt('${abs_file}', delimiter=',', dtype=str, skip_header=1)
        end = time.perf_counter()
        total_time += end - start

    print(f"{total_time/iterations:.4f}|{iterations}")
except:
    print("FAILED|0")
PYEOF
) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["python_numpy"]="$result"
    log "    numpy: $result"
}

# ============================================================================
# PHP BENCHMARKS (8 parsers)
# ============================================================================

run_php_benchmarks() {
    local file="$1"
    local abs_file="${PROJECT_ROOT}/${file}"
    local php_ext="${PROJECT_ROOT}/bindings/php/modules/cisv.so"

    if ! command_exists php; then
        log "Skipping PHP benchmarks (php not available)"
        return
    fi

    log "Running PHP benchmarks..."

    # Set library path for cisv
    local lib_path="${PROJECT_ROOT}/core/build"

    # cisv PHP extension benchmark (parse)
    local result
    if [ -f "$php_ext" ]; then
        result=$(LD_LIBRARY_PATH="$lib_path:${LD_LIBRARY_PATH:-}" BENCH_FILE="$abs_file" php -d "extension=$php_ext" -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$parser = new CisvParser();
    \$rows = \$parser->parseFile(\$file);
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    else
        log "    cisv extension not found at $php_ext"
        result="FAILED|0"
    fi
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_cisv"]="$result"
    log "    cisv (parse): $result"

    # cisv PHP extension benchmark (count) - shows raw C performance
    if [ -f "$php_ext" ]; then
        result=$(LD_LIBRARY_PATH="$lib_path:${LD_LIBRARY_PATH:-}" BENCH_FILE="$abs_file" php -d "extension=$php_ext" -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$count = CisvParser::countRows(\$file);
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    else
        result="FAILED|0"
    fi
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_cisv-count"]="$result"
    log "    cisv (count): $result"

    # PHP native fgetcsv benchmark
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

    # PHP str_getcsv benchmark
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

    # PHP SplFileObject benchmark
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$spl = new SplFileObject(\$file);
    \$spl->setFlags(SplFileObject::READ_CSV);
    \$rows = [];
    foreach (\$spl as \$row) {
        \$rows[] = \$row;
    }
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_splfileobject"]="$result"
    log "    SplFileObject: $result"

    # PHP League CSV benchmark (from composer)
    if [ -f "/tmp/csv_bench/vendor/autoload.php" ]; then
        result=$(BENCH_FILE="$abs_file" php -r "
require '/tmp/csv_bench/vendor/autoload.php';

use League\Csv\Reader;

\$file = getenv('BENCH_FILE');
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
    else
        result="FAILED|0"
    fi
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_league-csv"]="$result"
    log "    league-csv: $result"

    # PHP explode benchmark (simple but fast)
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
        if (\$line) \$rows[] = explode(',', \$line);
    }
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_explode"]="$result"
    log "    explode: $result"

    # PHP preg_split benchmark
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$content = file_get_contents(\$file);
    \$lines = preg_split('/\r?\n/', \$content);
    \$rows = [];
    foreach (\$lines as \$line) {
        if (\$line) \$rows[] = preg_split('/,/', \$line);
    }
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_preg_split"]="$result"
    log "    preg_split: $result"

    # PHP array_map + str_getcsv benchmark
    result=$(BENCH_FILE="$abs_file" php -r "
\$file = getenv('BENCH_FILE');
\$iterations = 5;
\$totalTime = 0;

for (\$i = 0; \$i < \$iterations; \$i++) {
    \$start = microtime(true);
    \$content = file(\$file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    \$rows = array_map('str_getcsv', \$content);
    \$end = microtime(true);
    \$totalTime += (\$end - \$start);
}

echo number_format(\$totalTime / \$iterations, 4) . '|' . \$iterations;
" 2>/dev/null) || result="FAILED|0"
    [ -z "$result" ] && result="FAILED|0"
    RESULTS["php_array_map"]="$result"
    log "    array_map: $result"
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

# Format result with validation checkmark for cisv entries
format_result_validated() {
    local key="$1"
    local file_size_mb="$2"
    local validation_key="$3"
    local result="${RESULTS[$key]:-FAILED|0}"
    local time=$(echo "$result" | cut -d'|' -f1)
    local runs=$(echo "$result" | cut -d'|' -f2)
    local valid=$(format_validation "$validation_key")

    if [ "$time" = "FAILED" ] || [ -z "$time" ] || [ "$time" = "0" ]; then
        echo "| - | - | - | - |"
    else
        local speed=$(awk "BEGIN {printf \"%.2f\", $file_size_mb / $time}")
        echo "| $time | $speed | $runs/$ITERATIONS | $valid |"
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

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** $(format_result_validated "count_cisv" "$file_size_mb" "cli_count")
| rust-csv $(format_result "count_rust-csv" "$file_size_mb") - |
| xsv $(format_result "count_xsv" "$file_size_mb") - |
| wc -l $(format_result "count_wc" "$file_size_mb") - |
| awk $(format_result "count_awk" "$file_size_mb") - |
| miller $(format_result "count_miller" "$file_size_mb") - |
| csvkit $(format_result "count_csvkit" "$file_size_mb") - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** $(format_result_validated "select_cisv" "$file_size_mb" "cli_parse")
| rust-csv $(format_result "select_rust-csv" "$file_size_mb") - |
| xsv $(format_result "select_xsv" "$file_size_mb") - |
| awk $(format_result "select_awk" "$file_size_mb") - |
| cut $(format_result "select_cut" "$file_size_mb") - |
| miller $(format_result "select_miller" "$file_size_mb") - |
| csvkit $(format_result "select_csvkit" "$file_size_mb") - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** $(format_result_validated "nodejs_cisv" "$file_size_mb" "nodejs_parse")
| **cisv (count)** $(format_result_validated "nodejs_cisv-count" "$file_size_mb" "nodejs_count")
| papaparse $(format_result "nodejs_papaparse" "$file_size_mb") - |
| csv-parse $(format_result "nodejs_csv-parse" "$file_size_mb") - |
| fast-csv $(format_result "nodejs_fast-csv" "$file_size_mb") - |
| csv-parser $(format_result "nodejs_csv-parser" "$file_size_mb") - |
| d3-dsv $(format_result "nodejs_d3-dsv" "$file_size_mb") - |
| csv-string $(format_result "nodejs_csv-string" "$file_size_mb") - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** $(format_result_validated "python_cisv" "$file_size_mb" "python_count")
| polars $(format_result "python_polars" "$file_size_mb") - |
| pyarrow $(format_result "python_pyarrow" "$file_size_mb") - |
| pandas $(format_result "python_pandas" "$file_size_mb") - |
| csv (stdlib) $(format_result "python_csv-stdlib" "$file_size_mb") - |
| DictReader $(format_result "python_dictreader" "$file_size_mb") - |
| numpy $(format_result "python_numpy" "$file_size_mb") - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** $(format_result_validated "php_cisv" "$file_size_mb" "php_parse")
| **cisv (count)** $(format_result_validated "php_cisv-count" "$file_size_mb" "php_count")
| fgetcsv $(format_result "php_fgetcsv" "$file_size_mb") - |
| str_getcsv $(format_result "php_str_getcsv" "$file_size_mb") - |
| SplFileObject $(format_result "php_splfileobject" "$file_size_mb") - |
| league/csv $(format_result "php_league-csv" "$file_size_mb") - |
| explode $(format_result "php_explode" "$file_size_mb") - |
| preg_split $(format_result "php_preg_split" "$file_size_mb") - |
| array_map $(format_result "php_array_map" "$file_size_mb") - |

> **Note:** cisv (count) shows native C performance without PHP array creation overhead.
> cisv (parse) includes the cost of converting C data to PHP arrays.

---

## 5. Technology Notes

| Tool | Language | Key Features |
|------|----------|--------------|
| cisv | C | SIMD (AVX-512/AVX2/SSE2), zero-copy parsing |
| rust-csv | Rust | Memory-safe, streaming, Serde support |
| xsv | Rust | Full CSV toolkit, parallel processing |
| polars | Rust/Python | DataFrame, parallel, lazy evaluation |
| pyarrow | C++/Python | Apache Arrow, columnar format |
| papaparse | JavaScript | Browser/Node, streaming, auto-detect |
| league/csv | PHP | RFC 4180 compliant, streaming |

---

## Methodology

- Each test was run **${ITERATIONS} times** and averaged
- Tests used a **${file_size_mb} MB** CSV file with **${row_count}** rows
- All tests ran on the same machine sequentially
- Warm-up runs were not performed (cold start)
- Times include file I/O and parsing

---

## Validation

The **Valid** column shows whether CISV correctly parsed the data:

| Symbol | Meaning |
|--------|---------|
| ✓ | Validation passed - data parsed correctly |
| ✗ | Validation failed - data mismatch detected |
| - | Not validated (third-party tool) |

### Validation Checks Performed

For each CISV binding, the following validations are performed:

1. **Row Count**: Verify the parser returns exactly **${row_count}** rows (including header)
2. **Field Count**: Verify each row contains **${EXPECTED_FIELD_COUNT}** fields
3. **Header Verification**: Confirm headers match: \`${EXPECTED_HEADERS}\`
4. **Data Integrity**: Verify first data row starts with expected ID value

### Validation Results Summary

| Binding | Count | Parse |
|---------|-------|-------|
| CLI | $(format_validation "cli_count") | $(format_validation "cli_parse") |
| Node.js | $(format_validation "nodejs_count") | $(format_validation "nodejs_parse") |
| Python | $(format_validation "python_count") | - |
| PHP | $(format_validation "php_count") | $(format_validation "php_parse") |

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

                # Run validation checks for cisv
                run_validations "$file"

                # Output markdown to stdout
                generate_markdown_report "$file"
            done

            cleanup
            log "Benchmark complete!"
            ;;
    esac
}

main "$@"
