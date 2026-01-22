#!/bin/bash
#
# CISV CLI Benchmark
# Compares cisv CLI performance against other CSV CLI tools
#

set -uo pipefail

# ============================================================================
# CONFIGURATION
# ============================================================================

ITERATIONS=${ITERATIONS:-5}
ROWS=${ROWS:-1000000}
COLS=${COLS:-7}
NO_CLEANUP=${NO_CLEANUP:-false}

# Results storage
declare -A RESULTS

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

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

# ============================================================================
# TEST DATA GENERATION
# ============================================================================

generate_csv() {
    local rows=$1
    local cols=$2
    local filename=$3

    log "Generating CSV: ${rows} rows × ${cols} columns..."

    python3 << PYEOF
import csv
import random
import string

def rs(n=10):
    return ''.join(random.choices(string.ascii_letters, k=n))

with open('$filename', 'w', newline='') as f:
    w = csv.writer(f)
    headers = ['col' + str(i) for i in range($cols)]
    w.writerow(headers)
    for i in range($rows):
        row = ['value_' + str(i) + '_' + str(j) for j in range($cols)]
        w.writerow(row)
PYEOF

    local size_mb=$(get_file_size_mb "$filename")
    log "  Done: ${size_mb} MB"
}

# ============================================================================
# BENCHMARK FUNCTION
# ============================================================================

run_benchmark() {
    local name="$1"
    local cmd="$2"
    local file="$3"
    local extra="${4:-}"

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
# CLI BENCHMARKS
# ============================================================================

run_cli_benchmarks() {
    local file="$1"
    local file_size_mb=$(get_file_size_mb "$file")

    # Find cisv binary
    local CISV_BIN=""
    if [ -f "./cli/build/cisv" ]; then
        CISV_BIN="./cli/build/cisv"
        export LD_LIBRARY_PATH="./core/build:${LD_LIBRARY_PATH:-}"
    elif [ -f "/benchmark/cisv/cli/build/cisv" ]; then
        CISV_BIN="/benchmark/cisv/cli/build/cisv"
        export LD_LIBRARY_PATH="/benchmark/cisv/core/build:${LD_LIBRARY_PATH:-}"
    elif [ -f "/usr/local/bin/cisv" ]; then
        CISV_BIN="/usr/local/bin/cisv"
    fi

    log ""
    log "============================================================"
    log "CLI BENCHMARK: $(echo "$file_size_mb" | xargs) MB file"
    log "============================================================"
    log ""

    # Row counting benchmarks
    log "--- Row Counting Benchmarks ---"
    log ""

    [ -n "$CISV_BIN" ] && {
        RESULTS["count_cisv"]=$(run_benchmark "cisv -c" "$CISV_BIN -c" "$file")
        log "  cisv: ${RESULTS[count_cisv]}"
    }

    command_exists xsv && {
        RESULTS["count_xsv"]=$(run_benchmark "xsv count" "xsv count" "$file")
        log "  xsv: ${RESULTS[count_xsv]}"
    }

    RESULTS["count_wc"]=$(run_benchmark "wc -l" "wc -l" "$file")
    log "  wc -l: ${RESULTS[count_wc]}"

    RESULTS["count_awk"]=$(run_benchmark "awk" "awk 'END{print NR}'" "$file")
    log "  awk: ${RESULTS[count_awk]}"

    command_exists mlr && {
        RESULTS["count_mlr"]=$(run_benchmark "miller" "mlr --csv count" "$file")
        log "  miller: ${RESULTS[count_mlr]}"
    }

    log ""

    # Column selection benchmarks
    log "--- Column Selection Benchmarks (cols 0,2,3) ---"
    log ""

    [ -n "$CISV_BIN" ] && {
        RESULTS["select_cisv"]=$(run_benchmark "cisv -s" "$CISV_BIN -s 0,2,3" "$file")
        log "  cisv: ${RESULTS[select_cisv]}"
    }

    command_exists xsv && {
        RESULTS["select_xsv"]=$(run_benchmark "xsv select" "xsv select 1,3,4" "$file")
        log "  xsv: ${RESULTS[select_xsv]}"
    }

    RESULTS["select_awk"]=$(run_benchmark "awk" "awk -F',' '{print \$1,\$3,\$4}'" "$file")
    log "  awk: ${RESULTS[select_awk]}"

    RESULTS["select_cut"]=$(run_benchmark "cut" "cut -d',' -f1,3,4" "$file")
    log "  cut: ${RESULTS[select_cut]}"

    command_exists mlr && {
        RESULTS["select_mlr"]=$(run_benchmark "miller" "mlr --csv cut -f col0,col2,col3" "$file")
        log "  miller: ${RESULTS[select_mlr]}"
    }

    log ""
}

# ============================================================================
# MARKDOWN REPORT
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
    local row_count=$(wc -l < "$file" | tr -d ' ')

    cat << EOF
# CISV CLI Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | $(date -u +"%Y-%m-%d %H:%M:%S UTC") |
| **File Size** | ${file_size_mb} MB |
| **Row Count** | ${row_count} |
| **Iterations** | ${ITERATIONS} |
| **Platform** | $(uname -s) $(uname -m) |

---

## Row Counting Performance

| Tool | Time (s) | Speed (MB/s) | Runs |
|------|----------|--------------|------|
| **cisv** $(format_result "count_cisv" "$file_size_mb")
| xsv $(format_result "count_xsv" "$file_size_mb")
| wc -l $(format_result "count_wc" "$file_size_mb")
| awk $(format_result "count_awk" "$file_size_mb")
| miller $(format_result "count_mlr" "$file_size_mb")

---

## Column Selection Performance

| Tool | Time (s) | Speed (MB/s) | Runs |
|------|----------|--------------|------|
| **cisv** $(format_result "select_cisv" "$file_size_mb")
| xsv $(format_result "select_xsv" "$file_size_mb")
| awk $(format_result "select_awk" "$file_size_mb")
| cut $(format_result "select_cut" "$file_size_mb")
| miller $(format_result "select_mlr" "$file_size_mb")

---

## Tool Descriptions

| Tool | Description |
|------|-------------|
| cisv | High-performance C CSV parser with SIMD optimizations |
| xsv | Rust-based CSV toolkit |
| wc -l | Unix line counter (baseline, counts newlines only) |
| awk | Text processing tool |
| cut | Column extraction tool (delimiter-based) |
| miller | Like awk/sed/cut for name-indexed data |

---

*Report generated by CISV CLI Benchmark*
EOF
}

# ============================================================================
# MAIN
# ============================================================================

show_help() {
    cat << EOF
CISV CLI Benchmark

Usage: $0 [OPTIONS]

Options:
    --rows=N        Number of rows to generate (default: 1000000)
    --cols=N        Number of columns (default: 7)
    --iterations=N  Benchmark iterations (default: 5)
    --file=PATH     Use existing CSV file instead of generating
    --no-cleanup    Keep test file after benchmark
    --help          Show this help

Environment Variables:
    ROWS            Same as --rows
    COLS            Same as --cols
    ITERATIONS      Same as --iterations
    NO_CLEANUP      Set to 'true' to keep test files

Examples:
    $0 --rows=1000000
    $0 --file=/data/large.csv
    ROWS=500000 ITERATIONS=10 $0

EOF
}

main() {
    local file=""

    while [[ $# -gt 0 ]]; do
        case $1 in
            --rows=*) ROWS="${1#*=}"; shift ;;
            --cols=*) COLS="${1#*=}"; shift ;;
            --iterations=*) ITERATIONS="${1#*=}"; shift ;;
            --file=*) file="${1#*=}"; shift ;;
            --no-cleanup) NO_CLEANUP=true; shift ;;
            --help|-h) show_help; exit 0 ;;
            *) shift ;;
        esac
    done

    # Generate or use existing file
    if [ -n "$file" ]; then
        if [ ! -f "$file" ]; then
            log "Error: File not found: $file"
            exit 1
        fi
    else
        file="benchmark_data.csv"
        generate_csv "$ROWS" "$COLS" "$file"
    fi

    # Run benchmarks
    run_cli_benchmarks "$file"

    # Generate markdown report to stdout
    generate_markdown_report "$file"

    # Cleanup
    if [ "$NO_CLEANUP" != "true" ] && [ -z "${file:-}" ]; then
        rm -f "benchmark_data.csv"
    fi

    log ""
    log "Benchmark complete!"
}

main "$@"
