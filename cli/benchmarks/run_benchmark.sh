#!/bin/bash
#
# CISV CLI Benchmark
#
# Compares cisv CLI performance against other popular CSV command-line tools.
#
# Tools compared:
# - cisv: High-performance C CSV parser with SIMD optimizations
# - qsv: Rust-based xsv fork with 80+ commands, SIMD-accelerated
# - xsv: Original Rust CSV toolkit
# - csvtk: Go-based cross-platform CSV/TSV toolkit
# - tsv-utils: D-based tools optimized for large datasets (eBay)
# - frawk: Rust AWK-like tool with native CSV support
# - miller: Go-based awk/sed/cut for name-indexed data
# - datamash: GNU statistical operations tool
# - goawk: Go AWK with native CSV support
# - csvkit: Python-based CSV utilities
# - wc/awk/cut: Unix baseline tools
#
# Usage:
#   ./run_benchmark.sh [OPTIONS]
#
# Options:
#   --rows=N        Number of rows to generate (default: 1000000)
#   --cols=N        Number of columns (default: 7)
#   --iterations=N  Benchmark iterations (default: 5)
#   --file=PATH     Use existing CSV file instead of generating
#   --fast          Skip slow tools (csvkit) and medium tools (miller, datamash, goawk)
#   --help          Show this help
#

set -o pipefail

# ============================================================================
# CONFIGURATION
# ============================================================================

ITERATIONS=5
ROWS=1000000
COLS=7
FAST_MODE=false
INPUT_FILE=""

# Temp file for results
RESULTS_FILE=""

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

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

format_number() {
    printf "%'d" "$1" 2>/dev/null || echo "$1"
}

# ============================================================================
# TEST DATA GENERATION
# ============================================================================

generate_csv() {
    local rows=$1
    local cols=$2
    local filename=$3

    echo "Generating CSV: $(format_number $rows) rows × ${cols} columns..."

    local start=$(date +%s.%N 2>/dev/null || date +%s)

    python3 << PYEOF
import sys

rows = $rows
cols = $cols
filename = '$filename'

with open(filename, 'w') as f:
    # Header
    f.write(','.join(['col' + str(i) for i in range(cols)]) + '\n')
    # Data rows
    for i in range(rows):
        f.write(','.join(['value_' + str(i) + '_' + str(j) for j in range(cols)]) + '\n')
        if i > 0 and i % 500000 == 0:
            print(f"  Generated {i:,} rows...", file=sys.stderr)
PYEOF

    local end=$(date +%s.%N 2>/dev/null || date +%s)
    local elapsed=$(awk "BEGIN {printf \"%.2f\", $end - $start}")
    local size=$(get_file_size "$filename")
    local size_mb=$(awk "BEGIN {printf \"%.1f\", $size / 1048576}")
    echo "  Done in ${elapsed}s, file size: ${size_mb} MB"
}

# ============================================================================
# BENCHMARK FUNCTION
# ============================================================================

run_benchmark() {
    local name="$1"
    local cmd="$2"
    local category="$3"

    echo "Benchmarking ${name}..."

    local times=()
    local row_count=0

    for i in $(seq 1 $ITERATIONS); do
        local start=$(date +%s.%N 2>/dev/null || date +%s)

        local output
        if output=$(timeout 300 bash -c "$cmd" 2>/dev/null); then
            local end=$(date +%s.%N 2>/dev/null || date +%s)
            local elapsed=$(awk "BEGIN {print $end - $start}")
            times+=("$elapsed")
            # Try to get row count from output (for counting operations)
            local trimmed=$(echo "$output" | tr -d '[:space:]')
            if [[ "$trimmed" =~ ^[0-9]+$ ]]; then
                row_count="$trimmed"
            fi
        else
            echo "  Error: command failed"
            return 1
        fi
    done

    if [ ${#times[@]} -eq 0 ]; then
        return 1
    fi

    # Calculate average
    local total=0
    for t in "${times[@]}"; do
        total=$(awk "BEGIN {print $total + $t}")
    done
    local avg=$(awk "BEGIN {printf \"%.3f\", $total / ${#times[@]}}")

    # Store result
    echo "$category|$name|$avg|$row_count" >> "$RESULTS_FILE"
}

# ============================================================================
# FORMAT OUTPUT
# ============================================================================

format_throughput() {
    local file_size=$1
    local time=$2

    if [ "$time" = "0" ] || [ -z "$time" ]; then
        echo "N/A"
    else
        local mb_per_sec=$(awk "BEGIN {printf \"%.1f\", ($file_size / 1048576) / $time}")
        echo "${mb_per_sec} MB/s"
    fi
}

print_results_table() {
    local category="$1"
    local file_size="$2"
    local default_rows="$3"

    echo ""
    echo "============================================================"
    echo "RESULTS: $category"
    echo "============================================================"
    printf "%-16s %12s %14s %12s\n" "Library" "Parse Time" "Throughput" "Rows"
    echo "------------------------------------------------------------"

    # Filter and sort results by time
    grep "^${category}|" "$RESULTS_FILE" 2>/dev/null | sort -t'|' -k3 -n | while IFS='|' read cat name time rows; do
        [ -z "$rows" ] || [ "$rows" = "0" ] && rows="$default_rows"
        local throughput=$(format_throughput "$file_size" "$time")
        printf "%-16s %10ss %14s %12s\n" "$name" "$time" "$throughput" "$(format_number $rows)"
    done
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
    --fast          Skip slow tools (csvkit) and medium tools (miller, datamash, goawk)
    --help          Show this help

Tool categories:
    Fast (always run):   cisv, qsv, xsv, csvtk, tsv-utils, frawk, wc, awk, cut
    Medium (skip --fast): miller, datamash, goawk
    Slow (skip --fast):   csvkit

Examples:
    $0 --rows=100000
    $0 --file=/data/large.csv
    $0 --rows=1000000 --fast

EOF
}

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --rows=*) ROWS="${1#*=}"; shift ;;
            --cols=*) COLS="${1#*=}"; shift ;;
            --iterations=*) ITERATIONS="${1#*=}"; shift ;;
            --file=*) INPUT_FILE="${1#*=}"; shift ;;
            --fast) FAST_MODE=true; shift ;;
            --help|-h) show_help; exit 0 ;;
            *) shift ;;
        esac
    done

    # Create temp file for results
    RESULTS_FILE=$(mktemp)
    trap "rm -f $RESULTS_FILE" EXIT

    echo "============================================================"
    echo "CISV CLI Benchmark"
    echo "============================================================"
    echo ""

    # Generate or use existing file
    local filepath
    local file_size

    if [ -n "$INPUT_FILE" ]; then
        filepath="$INPUT_FILE"
        if [ ! -f "$filepath" ]; then
            echo "Error: File not found: $filepath"
            exit 1
        fi
        file_size=$(get_file_size "$filepath")
        local size_mb=$(awk "BEGIN {printf \"%.1f\", $file_size / 1048576}")
        echo "Using existing file: $filepath (${size_mb} MB)"
    else
        filepath="/tmp/cisv_benchmark_$$.csv"
        generate_csv "$ROWS" "$COLS" "$filepath"
        file_size=$(get_file_size "$filepath")
    fi

    local size_mb=$(awk "BEGIN {printf \"%.1f\", $file_size / 1048576}")
    local row_count=$(wc -l < "$filepath" | tr -d ' ')

    echo ""
    echo "============================================================"
    echo "BENCHMARK: $(format_number $ROWS) rows × $COLS columns"
    echo "File size: ${size_mb} MB"
    echo "Iterations: $ITERATIONS"
    echo "Fast mode: $FAST_MODE"
    echo "============================================================"
    echo ""

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

    # ========================================================================
    # ROW COUNTING BENCHMARKS
    # ========================================================================

    echo "--- Row Counting Benchmarks ---"
    echo ""

    # ---- FAST TOOLS (always run) ----

    # cisv -c
    if [ -n "$CISV_BIN" ]; then
        run_benchmark "cisv" "$CISV_BIN -c \"$filepath\"" "count"
    else
        echo "Benchmarking cisv..."
        echo "  Skipped: cisv not available"
    fi

    # qsv count
    if command_exists qsv; then
        run_benchmark "qsv" "qsv count \"$filepath\"" "count"
    else
        echo "Benchmarking qsv..."
        echo "  Skipped: qsv not installed"
    fi

    # xsv count
    if command_exists xsv; then
        run_benchmark "xsv" "xsv count \"$filepath\"" "count"
    else
        echo "Benchmarking xsv..."
        echo "  Skipped: xsv not installed"
    fi

    # csvtk nrow
    if command_exists csvtk; then
        run_benchmark "csvtk" "csvtk nrow \"$filepath\"" "count"
    else
        echo "Benchmarking csvtk..."
        echo "  Skipped: csvtk not installed"
    fi

    # tsv-utils: tsv-summarize (needs CSV to be treated properly)
    if command_exists tsv-summarize; then
        run_benchmark "tsv-utils" "tsv-summarize --count \"$filepath\"" "count"
    else
        echo "Benchmarking tsv-utils..."
        echo "  Skipped: tsv-utils not installed"
    fi

    # frawk
    if command_exists frawk; then
        run_benchmark "frawk" "frawk -i csv 'END{print NR}' \"$filepath\"" "count"
    else
        echo "Benchmarking frawk..."
        echo "  Skipped: frawk not installed"
    fi

    # wc -l (baseline)
    run_benchmark "wc" "wc -l < \"$filepath\"" "count"

    # awk (baseline)
    run_benchmark "awk" "awk 'END{print NR}' \"$filepath\"" "count"

    # ---- MEDIUM TOOLS (skip with --fast) ----

    if [ "$FAST_MODE" != "true" ]; then
        # miller
        if command_exists mlr; then
            run_benchmark "miller" "mlr --csv count \"$filepath\"" "count"
        else
            echo "Benchmarking miller..."
            echo "  Skipped: miller not installed"
        fi

        # datamash
        if command_exists datamash; then
            run_benchmark "datamash" "datamash -H count 1 < \"$filepath\"" "count"
        else
            echo "Benchmarking datamash..."
            echo "  Skipped: datamash not installed"
        fi

        # goawk
        if command_exists goawk; then
            run_benchmark "goawk" "goawk --csv 'END{print NR}' \"$filepath\"" "count"
        else
            echo "Benchmarking goawk..."
            echo "  Skipped: goawk not installed"
        fi
    fi

    # ---- SLOW TOOLS (skip with --fast) ----

    if [ "$FAST_MODE" != "true" ]; then
        # csvkit (Python-based, slow)
        if command_exists csvstat; then
            run_benchmark "csvkit" "csvstat --count \"$filepath\"" "count"
        else
            echo "Benchmarking csvkit..."
            echo "  Skipped: csvkit not installed"
        fi
    fi

    # Print row counting results
    print_results_table "count" "$file_size" "$row_count"

    # ========================================================================
    # COLUMN SELECTION BENCHMARKS
    # ========================================================================

    echo ""
    echo "--- Column Selection Benchmarks ---"
    echo ""

    # ---- FAST TOOLS (always run) ----

    # cisv -s (0-indexed)
    if [ -n "$CISV_BIN" ]; then
        run_benchmark "cisv" "$CISV_BIN -s 0,2,3 \"$filepath\" | wc -l" "select"
    fi

    # qsv select (1-indexed)
    if command_exists qsv; then
        run_benchmark "qsv" "qsv select 1,3,4 \"$filepath\" | wc -l" "select"
    fi

    # xsv select (1-indexed)
    if command_exists xsv; then
        run_benchmark "xsv" "xsv select 1,3,4 \"$filepath\" | wc -l" "select"
    fi

    # csvtk cut (1-indexed)
    if command_exists csvtk; then
        run_benchmark "csvtk" "csvtk cut -f 1,3,4 \"$filepath\" | wc -l" "select"
    fi

    # tsv-select (1-indexed, works on CSV with -d)
    if command_exists tsv-select; then
        run_benchmark "tsv-utils" "tsv-select -d, -f 1,3,4 \"$filepath\" | wc -l" "select"
    fi

    # frawk (1-indexed with CSV mode)
    if command_exists frawk; then
        run_benchmark "frawk" "frawk -i csv -o csv '{print \$1,\$3,\$4}' \"$filepath\" | wc -l" "select"
    fi

    # cut (baseline, 1-indexed)
    run_benchmark "cut" "cut -d',' -f1,3,4 \"$filepath\" | wc -l" "select"

    # awk (baseline, 1-indexed)
    run_benchmark "awk" "awk -F',' '{print \$1,\$3,\$4}' \"$filepath\" | wc -l" "select"

    # ---- MEDIUM TOOLS (skip with --fast) ----

    if [ "$FAST_MODE" != "true" ]; then
        # miller
        if command_exists mlr; then
            run_benchmark "miller" "mlr --csv cut -f col0,col2,col3 \"$filepath\" | wc -l" "select"
        fi

        # goawk (1-indexed)
        if command_exists goawk; then
            run_benchmark "goawk" "goawk --csv '{print \$1,\$3,\$4}' \"$filepath\" | wc -l" "select"
        fi
    fi

    # ---- SLOW TOOLS (skip with --fast) ----

    if [ "$FAST_MODE" != "true" ]; then
        # csvkit (1-indexed)
        if command_exists csvcut; then
            run_benchmark "csvkit" "csvcut -c 1,3,4 \"$filepath\" | wc -l" "select"
        fi
    fi

    # Print column selection results
    print_results_table "select" "$file_size" "$row_count"

    # Cleanup
    if [ -z "$INPUT_FILE" ]; then
        rm -f "$filepath"
        echo ""
        echo "Cleaned up temporary file"
    fi

    echo ""
    echo "Benchmark complete!"
}

main "$@"
