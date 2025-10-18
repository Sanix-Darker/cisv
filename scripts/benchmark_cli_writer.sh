#!/bin/bash

# CISV Writer Benchmark Script
# Compares CSV writing performance

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== CSV Writer Benchmark ===${NC}\n"

# Check if cisv is built
if [ ! -f "./cisv" ]; then
    echo -e "${YELLOW}Building cisv CLI...${NC}"
    make clean
    make cli
fi

# Test file sizes
declare -A test_sizes=(
    ["small"]=1000
    ["medium"]=100000
    ["large"]=1000000
    ["xlarge"]=10000000
)

# Function to benchmark a write command
benchmark_write() {
    local name=$1
    local cmd=$2
    local output=$3
    local rows=$4

    echo -e "${BLUE}--- $name ---${NC}"

    # Get time with nanosecond precision
    start=$(date +%s.%N)
    eval "$cmd" > /dev/null 2>&1
    end=$(date +%s.%N)

    # Calculate elapsed time
    elapsed=$(echo "$end - $start" | bc)

    # Get file size
    if [ -f "$output" ]; then
        size=$(stat -c%s "$output" 2>/dev/null || stat -f%z "$output" 2>/dev/null)
        size_mb=$(echo "scale=2; $size / 1048576" | bc)
        throughput=$(echo "scale=2; $size_mb / $elapsed" | bc)
        rows_per_sec=$(echo "scale=0; $rows / $elapsed" | bc)

        echo "Time: ${elapsed} seconds"
        echo "Size: ${size_mb} MB"
        echo "Throughput: ${throughput} MB/s"
        echo "Rows/sec: ${rows_per_sec}"

        # Cleanup
        rm -f "$output"
    else
        echo "Error: Output file not created"
    fi
    echo ""
}

# Install dependencies if needed
install_deps() {
    echo -e "${YELLOW}Checking/installing dependencies...${NC}"

    # Check for Python pandas
    if ! python3 -c "import pandas" 2>/dev/null; then
        echo "Installing pandas..."
        pip3 install pandas
    fi

    # Check for Ruby
    if ! command -v ruby > /dev/null 2>&1; then
        echo "Ruby not found, skipping Ruby benchmarks"
    fi

    # Build generate_csv tool if needed
    if [ ! -f "./benchmark/generate_csv" ]; then
        echo "Building C comparison tool..."
        mkdir -p benchmark
        cat > benchmark/generate_csv.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <rows> <output>\n", argv[0]);
        return 1;
    }

    size_t rows = strtoull(argv[1], NULL, 10);
    FILE *out = fopen(argv[2], "w");
    if (!out) {
        perror("fopen");
        return 1;
    }

    // Write header
    fprintf(out, "id,name,email,value,timestamp\n");

    // Write rows
    for (size_t i = 0; i < rows; i++) {
        fprintf(out, "%zu,User_%zu,user%zu@example.com,%.2f,2024-01-01 00:00:00\n",
                i + 1, i, i, i * 1.23);
    }

    fclose(out);
    return 0;
}
EOF
        gcc -O3 -o benchmark/generate_csv benchmark/generate_csv.c
    fi

    echo ""
}

# Python CSV writer
create_python_writer() {
    cat > benchmark/write_csv.py << 'EOF'
import sys
import csv
from datetime import datetime

def generate_csv(rows, output):
    with open(output, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['id', 'name', 'email', 'value', 'timestamp'])

        for i in range(rows):
            writer.writerow([
                i + 1,
                f'User_{i}',
                f'user{i}@example.com',
                round(i * 1.23, 2),
                '2024-01-01 00:00:00'
            ])

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <rows> <output>")
        sys.exit(1)

    generate_csv(int(sys.argv[1]), sys.argv[2])
EOF
}

# Ruby CSV writer
create_ruby_writer() {
    cat > benchmark/write_csv.rb << 'EOF'
require 'csv'

if ARGV.length != 2
  puts "Usage: #{$0} <rows> <output>"
  exit 1
end

rows = ARGV[0].to_i
output = ARGV[1]

CSV.open(output, 'w') do |csv|
  csv << ['id', 'name', 'email', 'value', 'timestamp']

  rows.times do |i|
    csv << [
      i + 1,
      "User_#{i}",
      "user#{i}@example.com",
      (i * 1.23).round(2),
      '2024-01-01 00:00:00'
    ]
  end
end
EOF
}

# Node.js CSV writer using fast-csv
create_node_writer() {
    cat > benchmark/write_csv.js << 'EOF'
const fs = require('fs');
const fastcsv = require('fast-csv');

if (process.argv.length !== 4) {
    console.error(`Usage: ${process.argv[1]} <rows> <output>`);
    process.exit(1);
}

const rows = parseInt(process.argv[2]);
const output = process.argv[3];

const ws = fs.createWriteStream(output);
const csvStream = fastcsv.format({ headers: true });

csvStream.pipe(ws);

for (let i = 0; i < rows; i++) {
    csvStream.write({
        id: i + 1,
        name: `User_${i}`,
        email: `user${i}@example.com`,
        value: (i * 1.23).toFixed(2),
        timestamp: '2024-01-01 00:00:00'
    });
}

csvStream.end();
EOF
}

# Setup
install_deps
create_python_writer
create_ruby_writer
create_node_writer

# Run benchmarks for each size
for size_name in small medium large xlarge; do
    rows=${test_sizes[$size_name]}
    echo -e "${GREEN}=== Testing $size_name ($rows rows) ===${NC}\n"

    # CISV writer
    benchmark_write "cisv write" \
        "./cisv_bin write -g $rows -o bench_cisv.csv -b" \
        "bench_cisv.csv" \
        "$rows"

    # C fprintf baseline
    benchmark_write "C fprintf" \
        "./benchmark/generate_csv $rows bench_c.csv" \
        "bench_c.csv" \
        "$rows"

    # Python csv module
    benchmark_write "Python csv" \
        "python3 benchmark/write_csv.py $rows bench_python.csv" \
        "bench_python.csv" \
        "$rows"

    # Ruby CSV
    if command -v ruby > /dev/null 2>&1; then
        benchmark_write "Ruby CSV" \
            "ruby benchmark/write_csv.rb $rows bench_ruby.csv" \
            "bench_ruby.csv" \
            "$rows"
    fi

    # Node.js fast-csv (if available)
    if command -v node > /dev/null 2>&1 && [ -f "node_modules/fast-csv/index.js" ]; then
        benchmark_write "Node.js fast-csv" \
            "node benchmark/write_csv.js $rows bench_node.csv" \
            "bench_node.csv" \
            "$rows"
    fi

    # awk baseline
    benchmark_write "awk" \
        "awk 'BEGIN{print \"id,name,email,value,timestamp\"; for(i=1;i<=$rows;i++) printf \"%d,User_%d,user%d@example.com,%.2f,2024-01-01 00:00:00\\n\",i,i-1,i-1,(i-1)*1.23}' > bench_awk.csv" \
        "bench_awk.csv" \
        "$rows"

    echo -e "${BLUE}$(printf '=%.0s' {1..60})${NC}\n"

    # Only run small and medium for quick tests
    if [[ "$1" == "--quick" && "$size_name" == "medium" ]]; then
        break
    fi
done

# Cleanup
rm -f benchmark/write_csv.py benchmark/write_csv.rb benchmark/write_csv.js
rm -f benchmark/generate_csv.c benchmark/generate_csv

echo -e "${GREEN}Benchmark complete!${NC}"
