#!/bin/bash

# CISV CLI Benchmark Script
# Compares cisv performance with other popular CSV tools

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== CSV CLI Tools Benchmark ===${NC}\n"

# Check if cisv is built
if [ ! -f "./cisv" ]; then
    echo -e "${YELLOW}Building cisv CLI...${NC}"
    make clean  # Clean any old object files
    make cli
fi

# Check and build rust-csv benchmark if needed
if [ ! -f "./benchmark/rust-csv-bench/target/release/csv-bench" ]; then
    echo -e "${YELLOW}Building rust-csv benchmark tool...${NC}"
    make install-benchmark-deps
fi

# Create rust-csv select tool if it doesn't exist
if [ ! -f "./benchmark/rust-csv-bench/target/release/csv-select" ]; then
    echo -e "${YELLOW}Building rust-csv select tool...${NC}"
    mkdir -p benchmark/rust-csv-bench
    cd benchmark/rust-csv-bench
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

    // Write headers
    let headers = rdr.headers()?.clone();
    let selected_headers: Vec<&str> = columns.iter()
        .map(|&i| headers.get(i).unwrap_or(""))
        .collect();
    wtr.write_record(&selected_headers)?;

    // Write records
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
    if ! grep -q '^\[\[bin\]\]' Cargo.toml; then
        echo '[[bin]]' >> Cargo.toml
        echo 'name = "csv-select"' >> Cargo.toml
        echo 'path = "src/select.rs"' >> Cargo.toml
    fi
    cargo build --release --bin csv-select
    cd ../..
fi

# Generate test files
echo -e "${YELLOW}Generating test files...${NC}"

# Small file (1K rows)
echo "Creating small.csv (1K rows)..."
python3 -c "
import csv
with open('small.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(1000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

# Medium file (100K rows)
echo "Creating medium.csv (100K rows)..."
python3 -c "
import csv
with open('medium.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(100000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

# Large file (1M rows)
echo "Creating large.csv (1M rows)..."
python3 -c "
import csv
with open('large.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(1000000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

# xLarge file (10M rows)
echo "Creating xlarge.csv (10M rows)..."
python3 -c "
import csv
with open('xlarge.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(10000000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

echo -e "\n${GREEN}Test files created:${NC}"
ls -lh *.csv
echo ""

# Arrays to store benchmark results
declare -a BENCH_RESULTS
declare -a BENCH_NAMES
RESULT_COUNT=0

# Function to benchmark a command and store results
benchmark() {
    local name=$1
    local cmd=$2
    local file=$3

    # Special handling for commands that need to be checked differently
    local check_cmd="${cmd%% *}"

    # For complex commands, extract the actual binary name
    if [[ "$cmd" == *"./benchmark/rust-csv-bench"* ]]; then
        if [ -f "./benchmark/rust-csv-bench/target/release/csv-bench" ] || [ -f "./benchmark/rust-csv-bench/target/release/csv-select" ]; then
            check_cmd="exists"
        else
            check_cmd="not_exists"
        fi
    fi

    if [[ "$check_cmd" == "exists" ]] || command -v "$check_cmd" > /dev/null 2>&1; then
        echo -e "${BLUE}--- $name ---${NC}"
        # Run 3 times and calculate average
        local total_time=0
        local iterations=3

        for i in $(seq 1 $iterations); do
            local start=$(date +%s.%N)
            bash -c "$cmd $file > /dev/null 2>&1"
            local end=$(date +%s.%N)
            local elapsed=$(echo "$end - $start" | bc)
            total_time=$(echo "$total_time + $elapsed" | bc)
        done

        local avg_time=$(echo "scale=4; $total_time / $iterations" | bc)
        echo "Average Time: $avg_time seconds"

        # Calculate file size in MB
        local file_size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null)
        local file_size_mb=$(echo "scale=2; $file_size / 1048576" | bc)

        # Calculate speed (MB/s) and operations/sec
        local speed=$(echo "scale=2; $file_size_mb / $avg_time" | bc)
        local ops_per_sec=$(echo "scale=2; 1 / $avg_time" | bc)

        # Store results for later sorting
        BENCH_NAMES[$RESULT_COUNT]="$name"
        BENCH_RESULTS[$RESULT_COUNT]="$speed|$avg_time|$ops_per_sec"
        ((RESULT_COUNT++))

        # Memory measurement
        /usr/bin/time -f "Memory: %M KB" bash -c "$cmd $file > /dev/null 2>&1" 2>&1 | grep -v 'Command exited with non-zero status' || true
    else
        echo -e "${RED}$name: Not installed${NC}\n"
    fi
}

# Function to display sorted results table
display_sorted_results() {
    local test_name=$1

    echo -e "\n${GREEN}=== Sorted Results: $test_name ===${NC}"

    # Create temporary file for sorting
    local temp_file=$(mktemp)

    # Write all results to temp file
    for i in $(seq 0 $((RESULT_COUNT - 1))); do
        echo "${BENCH_RESULTS[$i]}|${BENCH_NAMES[$i]}" >> "$temp_file"
    done

    # Sort by Speed (MB/s) - descending
    echo -e "\n${YELLOW}Sorted by Speed (MB/s) - Fastest First:${NC}"
    echo "| Library            | Speed (MB/s) | Avg Time (s) | Operations/sec |"
    echo "|--------------------|--------------|--------------|----------------|"
    sort -t'|' -k1 -rn "$temp_file" | while IFS='|' read -r speed time ops name; do
        printf "| %-18s | %12.2f | %12.4f | %14.2f |\n" "$name" "$speed" "$time" "$ops"
    done

    # Sort by Operations/sec - descending
    echo -e "\n${YELLOW}Sorted by Operations/sec - Most Operations First:${NC}"
    echo "| Library            | Speed (MB/s) | Avg Time (s) | Operations/sec |"
    echo "|--------------------|--------------|--------------|----------------|"
    sort -t'|' -k3 -rn "$temp_file" | while IFS='|' read -r speed time ops name; do
        printf "| %-18s | %12.2f | %12.4f | %14.2f |\n" "$name" "$speed" "$time" "$ops"
    done

    rm "$temp_file"

    # Reset arrays for next test
    BENCH_RESULTS=()
    BENCH_NAMES=()
    RESULT_COUNT=0
}

# Run benchmarks for each file size
for size in "small" "medium" "large"; do
    echo -e "${GREEN}=== Testing with $size.csv ===${NC}\n"

    # Count rows test
    echo -e "${YELLOW}Row counting test:${NC}\n"

    benchmark "cisv" "./cisv_bin -c" "$size.csv"
    benchmark "wc -l (baseline)" "wc -l" "$size.csv"

    if [ -f "./benchmark/rust-csv-bench/target/release/csv-bench" ]; then
        benchmark "rust-csv (Rust)" "./benchmark/rust-csv-bench/target/release/csv-bench" "$size.csv"
    fi

    benchmark "csvkit (Python)" "csvstat --count" "$size.csv"
    benchmark "miller" "mlr --csv count" "$size.csv"

    # Display sorted results for row counting
    display_sorted_results "Row Counting - $size.csv"

    # Select columns test
    echo -e "${YELLOW}\n---\nColumn selection test (columns 0,2,3):${NC}\n"

    benchmark "cisv" "./cisv_bin -s 0,2,3" "$size.csv"

    if [ -f "./benchmark/rust-csv-bench/target/release/csv-select" ]; then
        benchmark "rust-csv" "./benchmark/rust-csv-bench/target/release/csv-select $size.csv 0,2,3" ""
    fi

    benchmark "csvkit" "csvcut -c 1,3,4" "$size.csv"
    benchmark "miller" "mlr --csv cut -f id,email,address" "$size.csv"

    # Display sorted results for column selection
    display_sorted_results "Column Selection - $size.csv"

    # Fixed separator line
    echo -e "${BLUE}==================================================${NC}\n"
done

# Special handling for xlarge file - only test cisv and wc
echo -e "${GREEN}=== Testing with xlarge.csv (limited tools) ===${NC}\n"

echo -e "${YELLOW}Row counting test:${NC}\n"
benchmark "cisv" "./cisv_bin -c" "xlarge.csv"
benchmark "wc -l (baseline)" "wc -l" "xlarge.csv"

# Display sorted results for xlarge row counting
display_sorted_results "Row Counting - xlarge.csv"

echo -e "${YELLOW}Column selection test (columns 0,2,3):${NC}\n"
benchmark "cisv" "./cisv_bin -s 0,2,3" "xlarge.csv"

# Display sorted results for xlarge column selection
display_sorted_results "Column Selection - xlarge.csv"

echo -e "${BLUE}==================================================${NC}\n"

# cisv specific benchmark mode with formatted output
echo -e "${GREEN}=== CISV Benchmark Mode (Detailed) ===${NC}\n"
echo "| File Size  | Speed (MB/s) | Avg Time (ms) | Operations/sec |"
echo "|------------|--------------|---------------|----------------|"
for size in "small" "medium" "large" "xlarge"; do
    echo -n "| $(printf '%-10s' "$size.csv") | "
    ./cisv_bin -b "$size.csv" | tail -1 | awk '{printf "%12s | %13s | %14s |\n", $3, $5, $7}'
done

# Cleanup
echo -e "${YELLOW}Cleaning up test files...${NC}"
rm -f small.csv medium.csv large.csv xlarge.csv

echo -e "${GREEN}Benchmark complete!${NC}"
