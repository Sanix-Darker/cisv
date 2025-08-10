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

# Function to benchmark a command
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
        # Run 3 times and show average
        bash -c "start=\$(date +%s.%N); for i in {1..3}; do $cmd $file > /dev/null 2>&1; done; end=\$(date +%s.%N); echo \"Time: \$(echo \"\$end - \$start\" | bc) seconds\"; "
        # Memory measurement with error suppression
        /usr/bin/time -f "Memory: %M KB" bash -c "$cmd $file > /dev/null 2>&1" 2>&1 | grep -v 'Command exited with non-zero status' || true
    else
        echo -e "${RED}$name: Not installed${NC}\n"
    fi
}

# Run benchmarks for each file size
for size in "small" "medium" "large"; do  # Skip xlarge for all tools
    echo -e "${GREEN}=== Testing with $size.csv ===${NC}\n"

    # Count rows test
    echo -e "${YELLOW}Row counting test:${NC}\n"

    benchmark "cisv" "./cisv -c" "$size.csv"
    benchmark "wc -l (baseline)" "wc -l" "$size.csv"

    if [ -f "./benchmark/rust-csv-bench/target/release/csv-bench" ]; then
        benchmark "rust-csv (Rust library)" "./benchmark/rust-csv-bench/target/release/csv-bench" "$size.csv"
    fi

    # benchmark "xsv (Rust CLI)" "xsv count" "$size.csv"
    # benchmark "qsv (faster xsv fork)" "qsv count" "$size.csv"
    benchmark "csvkit (Python)" "csvstat --count" "$size.csv"
    benchmark "miller" "mlr --csv count" "$size.csv"

    # Select columns test
    echo -e "${YELLOW}\n---\nColumn selection test (columns 0,2,3):${NC}\n"

    benchmark "cisv" "./cisv -s 0,2,3" "$size.csv"

    if [ -f "./benchmark/rust-csv-bench/target/release/csv-select" ]; then
        benchmark "rust-csv" "./benchmark/rust-csv-bench/target/release/csv-select $size.csv 0,2,3" ""
    fi

    # benchmark "xsv" "xsv select 1,3,4" "$size.csv"  # xsv uses 1-based indexing
    # benchmark "qsv" "qsv select 1,3,4" "$size.csv"  # qsv uses 1-based indexing
    benchmark "csvkit" "csvcut -c 1,3,4" "$size.csv"
    benchmark "miller" "mlr --csv cut -f id,email,address" "$size.csv"

    # Fixed separator line
    echo -e "${BLUE}==================================================${NC}\n"
done

# Special handling for xlarge file - only test cisv and wc
echo -e "${GREEN}=== Testing with xlarge.csv (limited tools) ===${NC}\n"

echo -e "${YELLOW}Row counting test:${NC}\n"
benchmark "cisv" "./cisv -c" "xlarge.csv"
benchmark "wc -l (baseline)" "wc -l" "xlarge.csv"

echo -e "${YELLOW}Column selection test (columns 0,2,3):${NC}\n"
benchmark "cisv" "./cisv -s 0,2,3" "xlarge.csv"

echo -e "${BLUE}==================================================${NC}\n"

# cisv specific benchmark mode
echo -e "${GREEN}=== CISV Benchmark Mode ===${NC}\n"
for size in "small" "medium" "large" "xlarge"; do
    echo -e "${YELLOW}$size.csv:${NC}"
    ./cisv -b "$size.csv"
    echo ""
done

# Cleanup
echo -e "${YELLOW}Cleaning up test files...${NC}"
rm -f small.csv medium.csv large.csv xlarge.csv

echo -e "${GREEN}Benchmark complete!${NC}"
