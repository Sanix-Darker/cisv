#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Arrays to store benchmark results
declare -a BENCH_RESULTS
declare -a BENCH_NAMES
RESULT_COUNT=0

# Function to benchmark a command and store results
benchmark() {
    local name=$1
    local cmd=$2
    local file=$3

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

echo -e "${BLUE}=== CSV CLI Tools Benchmark ===${NC}\n"

# Run benchmarks for each file size
for size in "small" "medium" "large"; do
    if [ ! -f "${size}.csv" ]; then
        echo -e "${RED}Warning: ${size}.csv not found, skipping...${NC}"
        continue
    fi

    echo -e "${GREEN}=== Testing with ${size}.csv ===${NC}\n"

    # Row counting test
    echo -e "${YELLOW}Row counting test:${NC}\n"

    benchmark "cisv" "./cisv_bin -c" "${size}.csv"
    benchmark "wc -l" "wc -l" "${size}.csv"

    # Add other tools if they exist
    if command -v csvstat > /dev/null 2>&1; then
        benchmark "csvkit" "csvstat --count" "${size}.csv"
    fi

    if command -v mlr > /dev/null 2>&1; then
        benchmark "miller" "mlr --csv count" "${size}.csv"
    fi

    # Display sorted results
    display_sorted_results "Row Counting - ${size}.csv"

    # Column selection test
    echo -e "\n${YELLOW}Column selection test (columns 0,2,3):${NC}\n"

    benchmark "cisv" "./cisv_bin -s 0,2,3" "${size}.csv"

    if command -v csvcut > /dev/null 2>&1; then
        benchmark "csvkit" "csvcut -c 1,3,4" "${size}.csv"
    fi

    if command -v mlr > /dev/null 2>&1; then
        benchmark "miller" "mlr --csv cut -f id,email,address" "${size}.csv"
    fi

    # Display sorted results
    display_sorted_results "Column Selection - ${size}.csv"

    echo -e "${BLUE}==================================================${NC}\n"
done

echo -e "${GREEN}Benchmark complete!${NC}"
