#!/bin/bash
# CISV Performance Test Suite
# Comprehensive benchmarking for all optimizations

set -e

# Configuration
LARGE_CSV="${LARGE_CSV:-large.csv}"
ITERATIONS="${ITERATIONS:-5}"
RESULTS_DIR="${RESULTS_DIR:-perf_results}"
CISV_BIN="${CISV_BIN:-./cli/build/cisv}"

mkdir -p "$RESULTS_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Generate test data if needed
generate_test_data() {
    if [ ! -f "$LARGE_CSV" ]; then
        log_info "Generating 1M row test file..."
        python3 -c "
import csv
import random
import string
with open('$LARGE_CSV', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id','name','email','value','description'])
    for i in range(1000000):
        w.writerow([
            i,
            ''.join(random.choices(string.ascii_letters, k=20)),
            f'user{i}@example.com',
            random.uniform(0, 10000),
            ''.join(random.choices(string.ascii_letters + ' ', k=50))
        ])
"
        log_info "Generated: $(du -h $LARGE_CSV)"
    else
        log_info "Using existing test file: $(du -h $LARGE_CSV)"
    fi
}

# Benchmark function
benchmark() {
    local name="$1"
    local cmd="$2"
    local total=0

    echo ""
    log_info "Benchmarking: $name"
    for i in $(seq 1 $ITERATIONS); do
        start=$(date +%s.%N)
        eval "$cmd" > /dev/null 2>&1
        end=$(date +%s.%N)
        elapsed=$(echo "$end - $start" | bc)
        total=$(echo "$total + $elapsed" | bc)
        printf "  Run %d: %.4fs\n" "$i" "$elapsed"
    done

    avg=$(echo "scale=4; $total / $ITERATIONS" | bc)
    file_mb=$(du -m "$LARGE_CSV" | cut -f1)
    speed=$(echo "scale=2; $file_mb / $avg" | bc)

    echo "$(date +%s),$name,$avg,$speed" >> "$RESULTS_DIR/benchmark_results.csv"
    printf "  ${GREEN}Average: %.4fs (%.2f MB/s)${NC}\n" "$avg" "$speed"
}

# Main
main() {
    echo "=============================================="
    echo "   CISV Performance Test Suite"
    echo "=============================================="
    echo ""

    # Check for cisv binary
    if [ ! -f "$CISV_BIN" ]; then
        log_error "CISV binary not found at $CISV_BIN"
        log_info "Building..."
        make -C "$(dirname $0)/.." all
    fi

    generate_test_data

    echo "timestamp,test,avg_time_s,speed_mbs" > "$RESULTS_DIR/benchmark_results.csv"

    # Core C benchmarks
    log_info "Running core benchmarks..."
    benchmark "cisv_parse" "$CISV_BIN parse $LARGE_CSV"
    benchmark "cisv_count" "$CISV_BIN count $LARGE_CSV"
    benchmark "cisv_select_3cols" "$CISV_BIN select -c 1,2,3 $LARGE_CSV"

    # Memory profiling (if valgrind available)
    if command -v valgrind &> /dev/null; then
        log_info "Running memory profiling..."
        valgrind --tool=massif --massif-out-file="$RESULTS_DIR/massif.out" \
            "$CISV_BIN" parse "$LARGE_CSV" 2>/dev/null || true

        if [ -f "$RESULTS_DIR/massif.out" ] && command -v ms_print &> /dev/null; then
            ms_print "$RESULTS_DIR/massif.out" > "$RESULTS_DIR/massif_report.txt" 2>/dev/null || true
        fi
    else
        log_warn "valgrind not available, skipping memory profiling"
    fi

    echo ""
    echo "=============================================="
    log_info "Results saved to $RESULTS_DIR/"
    echo "=============================================="
}

main "$@"
