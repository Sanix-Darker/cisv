#!/bin/bash

set -uo pipefail
trap 'echo "Error occurred at line $LINENO. Exit code: $?"' ERR

# Colors (should be uncomment from direct run not from cli"
# RED='\033[0;31m'
# GREEN='\033[0;32m'
# YELLOW='\033[1;33m'
# BLUE='\033[0;34m'
# NC='\033[0m'

RED=""
GREEN=""
YELLOW=""
BLUE=""
NC=""

# Configuration
ITERATIONS=3
SIZES=("small")
declare -a BENCH_RESULTS
declare -a BENCH_NAMES
RESULT_COUNT=0

# Initialize optional variables
FORCE_REGENERATE=false
NO_CLEANUP=false

print_msg() {
    echo -e "${1}${2}${NC}"
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

detect_os() {
    OS="unknown"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    fi
}

install_rust() {
    if ! command_exists cargo; then
        print_msg "$YELLOW" "Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
        source $HOME/.cargo/env
    else
        print_msg "$GREEN" "✓ Rust already installed"
    fi
}

build_rust_csv_tools() {
    print_msg "$YELLOW" "Building rust-csv benchmark tools..."
    mkdir -p benchmark/rust-csv-bench
    cd benchmark/rust-csv-bench

    if [ ! -f Cargo.toml ]; then
        cargo init --name csv-bench
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
    fi

    mkdir -p src
    cat > src/main.rs << 'EOF'
use std::env;
use std::error::Error;
use csv::ReaderBuilder;

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <file>", args[0]);
        std::process::exit(1);
    }
    let mut rdr = ReaderBuilder::new()
        .has_headers(true)
        .from_path(&args[1])?;
    let count = rdr.records().count();
    println!("{}", count);
    Ok(())
}
EOF

    cat > src/select.rs << 'EOF'
use std::env;
use std::error::Error;
use csv::{ReaderBuilder, WriterBuilder};

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("Usage: {} <file> <columns>", args[0]);
        std::process::exit(1);
    }
    let columns: Vec<usize> = args[2].split(',')
        .map(|s| s.parse::<usize>().unwrap())
        .collect();
    let mut rdr = ReaderBuilder::new()
        .has_headers(true)
        .from_path(&args[1])?;
    let mut wtr = WriterBuilder::new()
        .from_writer(std::io::stdout());
    let headers = rdr.headers()?.clone();
    let selected_headers: Vec<&str> = columns.iter()
        .map(|&i| headers.get(i).unwrap_or(""))
        .collect();
    wtr.write_record(&selected_headers)?;
    for result in rdr.records() {
        let record = result?;
        let selected: Vec<&str> = columns.iter()
            .map(|&i| record.get(i).unwrap_or(""))
            .collect();
        wtr.write_record(&selected)?;
    }
    wtr.flush()?;
    Ok(())
}
EOF

    cargo build --release
    cd ../..
    print_msg "$GREEN" "✓ rust-csv benchmark tools built"
}

install_csvkit() {
    if command_exists pip3; then
        if ! command_exists csvcut; then
            print_msg "$YELLOW" "Installing csvkit..."
            pip3 install csvkit
        else
            print_msg "$GREEN" "✓ csvkit already installed"
        fi
    else
        print_msg "$YELLOW" "⚠ pip3 not found, skipping csvkit installation"
    fi
}

install_dependencies() {
    print_msg "$BLUE" "Installing CSV parsing tool dependencies..."
    detect_os
    install_rust
    build_rust_csv_tools
    install_csvkit

    print_msg "$GREEN" "\nInstallation complete! Installed tools:"
    print_msg "$GREEN" "-----------------------------------"
    [ -f "benchmark/rust-csv-bench/target/release/csv-bench" ] && print_msg "$GREEN" "✓ rust-csv (benchmark tool)"
    command_exists csvcut && print_msg "$GREEN" "✓ csvkit"
}

get_file_size() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        stat -f%z "$1" 2>/dev/null
    else
        stat -c%s "$1" 2>/dev/null
    fi
}

benchmark() {
    local name=$1
    local cmd=$2
    local file=$3
    local extra_args="${4:-}"

    print_msg "$BLUE" ""
    print_msg "$BLUE" "-> $name"

    if [ ! -f "$file" ]; then
        print_msg "$RED" "Error: File $file not found"
        return 1
    fi

    local total_time=0
    local successful_runs=0

    for i in $(seq 1 $ITERATIONS); do
        local start_time=$(date +%s.%N 2>/dev/null || date +%s)
        local temp_output=$(mktemp)
        local temp_error=$(mktemp)

        # Build the full command with optional extra arguments
        local full_cmd="$cmd \"$file\""
        if [ -n "$extra_args" ]; then
            full_cmd="$cmd \"$file\" $extra_args"
        fi

        # Use timeout command if available, otherwise run directly
        if command_exists timeout; then
            timeout 30 bash -c "$full_cmd" > "$temp_output" 2> "$temp_error"
            local exit_status=$?

            if [ $exit_status -eq 124 ]; then
                print_msg "$RED" "  Run $i timed out after 30 seconds"
                rm -f "$temp_output" "$temp_error"
                continue
            fi
        else
            # Run without timeout protection
            bash -c "$full_cmd" > "$temp_output" 2> "$temp_error"
            local exit_status=$?
        fi

        rm -f "$temp_output" "$temp_error"

        if [ "$exit_status" -eq 0 ]; then
            local end_time=$(date +%s.%N 2>/dev/null || date +%s)
            local elapsed=$(awk "BEGIN {print $end_time - $start_time}")
            total_time=$(awk "BEGIN {print $total_time + $elapsed}")
            successful_runs=$((successful_runs + 1))
            echo "  Run $i: ${elapsed}s"
        else
            print_msg "$RED" "  Run $i failed with exit code $exit_status"
        fi
    done

    if [ $successful_runs -eq 0 ]; then
        print_msg "$RED" "  All runs failed for $name"
        return 1
    fi

    local avg_time=$(awk "BEGIN {printf \"%.4f\", $total_time / $successful_runs}")
    echo "  Average Time: ${avg_time} seconds"
    echo "  Successful runs: $successful_runs/$ITERATIONS"

    local file_size=$(get_file_size "$file")
    if [ -z "$file_size" ] || [ "$file_size" = "0" ]; then
        file_size=1
    fi
    local file_size_mb=$(awk "BEGIN {printf \"%.2f\", $file_size / 1048576}")

    # Safe division with awk
    local speed="0.00"
    local ops_per_sec="0.00"
    if [ -n "$avg_time" ] && [ "$avg_time" != "0" ]; then
        speed=$(awk "BEGIN {if ($avg_time > 0) printf \"%.2f\", $file_size_mb / $avg_time; else print \"0.00\"}")
        ops_per_sec=$(awk "BEGIN {if ($avg_time > 0) printf \"%.2f\", 1 / $avg_time; else print \"0.00\"}")
    fi

    BENCH_NAMES[$RESULT_COUNT]="$name"
    BENCH_RESULTS[$RESULT_COUNT]="$speed|$avg_time|$ops_per_sec"
    RESULT_COUNT=$((RESULT_COUNT + 1))
}

# Initialize arrays at the beginning of your script
initialize_benchmark_arrays() {
    declare -g -a BENCH_SPEEDS=()
    declare -g -a BENCH_TIMES=()
    declare -g -a BENCH_OPS=()
    declare -g -a BENCH_NAMES=()
    declare -g RESULT_COUNT=0
}

# Function to add benchmark results
add_benchmark_result() {
    local speed="$1"
    local time="$2"
    local ops="$3"
    local name="$4"

    BENCH_SPEEDS[${RESULT_COUNT}]="$speed"
    BENCH_TIMES[${RESULT_COUNT}]="$time"
    BENCH_OPS[${RESULT_COUNT}]="$ops"
    BENCH_NAMES[${RESULT_COUNT}]="$name"

    ((RESULT_COUNT++))
}

display_sorted_results() {
    local test_name=$1
    print_msg "$GREEN" "\n=== Sorted Results: $test_name ==="

    if [ ${RESULT_COUNT:-0} -eq 0 ]; then
        print_msg "$YELLOW" "No results to display"
        return
    fi

    local temp_file=$(mktemp)
    trap "rm -f $temp_file" EXIT

    # If BENCH_RESULTS contains "speed|time|ops" format
    for i in $(seq 0 $((RESULT_COUNT - 1))); do
        local result="${BENCH_RESULTS[$i]:-0|0|0}"
        local name="${BENCH_NAMES[$i]:-unknown}"
        echo "${result}|${name}" >> "$temp_file"
    done

    # Sort by Speed (MB/s) - Fastest First
    print_msg "$YELLOW" "\nSorted by Speed (MB/s) - Fastest First:"
    printf "| %-20s | %-12s | %-12s | %-14s |\n" "Library" "Speed (MB/s)" "Avg Time (s)" "Operations/sec"
    printf "|%-22s|%-14s|%-14s|%-16s|\n" "----------------------" "--------------" "--------------" "----------------"

    sort -t'|' -k1 -rn "$temp_file" | while IFS='|' read -r speed time ops name; do
        speed=$(printf "%.2f" "$speed" 2>/dev/null || echo "0.00")
        time=$(printf "%.4f" "$time" 2>/dev/null || echo "0.0000")
        ops=$(printf "%.2f" "$ops" 2>/dev/null || echo "0.00")
        printf "| %-20s | %12s | %12s | %14s |\n" "$name" "$speed" "$time" "$ops"
    done

    echo ""  # Empty line between tables

    # Sort by Operations/sec - Most Operations First
    print_msg "$YELLOW" "\nSorted by Operations/sec - Most Operations First:"
    printf "| %-20s | %-12s | %-12s | %-14s |\n" "Library" "Speed (MB/s)" "Avg Time (s)" "Operations/sec"
    printf "|%-22s|%-14s|%-14s|%-16s|\n" "----------------------" "--------------" "--------------" "----------------"

    sort -t'|' -k3 -rn "$temp_file" | while IFS='|' read -r speed time ops name; do
        speed=$(printf "%.2f" "$speed" 2>/dev/null || echo "0.00")
        time=$(printf "%.4f" "$time" 2>/dev/null || echo "0.0000")
        ops=$(printf "%.2f" "$ops" 2>/dev/null || echo "0.00")
        printf "| %-20s | %12s | %12s | %14s |\n" "$name" "$speed" "$time" "$ops"
    done

    rm -f "$temp_file"

    # Clear arrays for next run
    BENCH_RESULTS=()
    BENCH_NAMES=()
    RESULT_COUNT=0
}

generate_csv() {
    local rows=$1
    local filename=$2

    python3 << EOF
import csv
import random
import string

def random_string(length=10):
    return ''.join(random.choices(string.ascii_letters, k=length))

with open('$filename', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range($rows):
        if $rows >= 100000 and i % 100000 == 0:
            print(f"  Progress: {i/$rows*100:.0f}%")
        w.writerow([
            i,
            f'Person {random_string(8)} {i}',
            f'person{i}@email.com',
            f'{i} {random_string(6)} St, City {i%100}',
            f'555-{i:04d}',
            f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}',
            f'{i*1.23:.2f}'
        ])
EOF
}

generate_test_files() {
    print_msg "$YELLOW" "Generating test CSV files..."

    if ! command_exists python3; then
        print_msg "$RED" "Error: Python3 is required to generate test files"
        exit 1
    fi

    # if [ ! -f "small.csv" ] || [ "$FORCE_REGENERATE" = "true" ]; then
    #     echo "Creating small.csv (1K rows)..."
    #     generate_csv 1000 "small.csv"
    # fi

    # if ([ ! -f "medium.csv" ] || [ "$FORCE_REGENERATE" = "true" ]) && [[ " ${SIZES[@]} " =~ " medium " ]]; then
    #     echo "Creating medium.csv (100K rows)..."
    #     generate_csv 100000 "medium.csv"
    # fi

    if ([ ! -f "large.csv" ] || [ "$FORCE_REGENERATE" = "true" ]) && [[ " ${SIZES[@]} " =~ " large " ]]; then
        echo "Creating large.csv (1M rows)..."
        generate_csv 1000000 "large.csv"
    fi
}

install_cli_tools() {
    # linux/deb related
    apt-get install miller csvkit -y
}

run_cli_benchmarks() {
    install_cli_tools

    print_msg "$BLUE" "\n## CLI BENCHMARKS\n"

    for size in "${SIZES[@]}"; do
        if [ ! -f "${size}.csv" ]; then
            print_msg "$RED" "Warning: ${size}.csv not found, skipping..."
            continue
        fi

        print_msg "$GREEN" "\n### Testing with ${size}.csv\n"

        local file_size=$(get_file_size "${size}.csv")
        local file_size_mb=$(awk "BEGIN {printf \"%.2f\", $file_size / 1048576}")
        local row_count=$(wc -l < "${size}.csv")
        print_msg "$BLUE" "File info: ${file_size_mb} MB, ${row_count} rows\n"

        print_msg "$YELLOW" "#### ROW COUNTING TEST\n"

        print_msg "$RED" "\`\`\`"
        print_msg "$RED" ""
        # -------
        # bench for cisv:
        benchmark "cisv" "./cisv_bin -c" "${size}.csv"

        benchmark "rust-csv" "./benchmark/rust-csv-bench/target/release/csv-bench" "${size}.csv"

        benchmark "wc -l" "wc -l" "${size}.csv"

        benchmark "csvkit" "csvstat --count" "${size}.csv"

        benchmark "miller" "mlr --csv --headerless-csv-output cat -n then stats1 -a max -f n " "${size}.csv"
        print_msg "$RED" ""
        print_msg "$RED" "\`\`\`"

        display_sorted_results "Row Counting - ${size}.csv"

        print_msg "$YELLOW" "\n#### COLUMN SELECTION TEST (COLUMNS 0,2,3)\n"
        print_msg "$RED" "\`\`\`"
        print_msg "$RED" ""

        # -------
        # bench for cisv:
        benchmark "cisv" "./cisv_bin -s 0,2,3" "${size}.csv"

        if [ -f "benchmark/rust-csv-bench/target/release/csv-select" ]; then
            benchmark "rust-csv" "./benchmark/rust-csv-bench/target/release/csv-select" "${size}.csv" "0,2,3"
        fi

        if command_exists csvcut; then
            benchmark "csvkit" "csvcut -c 1,3,4" "${size}.csv"
        fi

        if command_exists mlr; then
            benchmark "miller" "mlr --csv cut -f id,email,address" "${size}.csv"
        fi

        print_msg "$RED" ""
        print_msg "$RED" "\`\`\`"

        display_sorted_results "Column Selection - ${size}.csv"
    done
}

run_npm_benchmarks() {
    print_msg "$BLUE" "\n## NPM Benchmarks\n"
    if [ -f "package.json" ] && command_exists npm; then
        npm run benchmark-js
    else
        print_msg "$RED" "Error: package.json not found or npm not installed"
        exit 1
    fi
}

cleanup() {
    local files=("small.csv" "medium.csv" "large.csv")
    print_msg "$YELLOW" "\nCleaning up test files..."
    for file in "${files[@]}"; do
        if [ -f "$file" ]; then
            rm -f "$file"
            echo "  Removed $file"
        fi
    done
}

show_help() {
    cat << EOF
Usage: $0 [MODE] [OPTIONS]

Modes:
    install           Install dependencies and build tools
    benchmark         Run CLI benchmarks
    npm-benchmark     Run npm benchmark

Options:
    --large           Include large (1M rows) file in benchmarks
    --all             Test all file sizes (small, medium, large)
    --force-regenerate Force regeneration of test files
    --no-cleanup      Don't remove test files after benchmark
    --debug           Enable debug mode (verbose output)
    --help            Show this help message

Examples:
    $0 install                    # Install dependencies
    $0 benchmark                  # Run CLI benchmarks
    $0 npm-benchmark              # Run npm benchmark
    $0 benchmark --all            # Run benchmarks with all file sizes

EOF
}

main() {
    MODE=""

    if [ $# -eq 0 ]; then
        show_help
        exit 0
    fi

    case $1 in
        install)
            MODE="install"
            shift
            ;;
        benchmark)
            MODE="benchmark"
            shift
            ;;
        npm-benchmark)
            MODE="npm-benchmark"
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_msg "$RED" "Unknown mode: $1"
            show_help
            exit 1
            ;;
    esac

    while [[ $# -gt 0 ]]; do
        case $1 in
            --large)
                SIZES+=("large")
                shift
                ;;
            --all)
                SIZES=("large") # "small" "medium" "large"
                shift
                ;;
            --force-regenerate)
                FORCE_REGENERATE=true
                shift
                ;;
            --no-cleanup)
                NO_CLEANUP=true
                shift
                ;;
            --debug)
                set -x
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                print_msg "$RED" "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    case $MODE in
        install)
            install_dependencies
            ;;
        benchmark)
            if ! command_exists awk; then
                print_msg "$RED" "Error: 'awk' is required but not installed"
                exit 1
            fi
            generate_test_files
            # cli benchs
            run_cli_benchmarks
            # npm benchs
            run_npm_benchmarks
            if [ "$NO_CLEANUP" != "true" ]; then
                cleanup
            fi
            print_msg "$GREEN" "\nBenchmark complete!"
            ;;
        npm-benchmark)
            run_npm_benchmarks
            ;;
    esac
}

main "$@"
