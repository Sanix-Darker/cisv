#!/bin/bash
# Run All Performance Tests

echo "=============================================="
echo "   CISV Full Performance Test Suite"
echo "=============================================="
echo ""

SCRIPTS_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="${RESULTS_DIR:-perf_results_$(date +%Y%m%d_%H%M%S)}"

mkdir -p "$RESULTS_DIR"

export LARGE_CSV="${LARGE_CSV:-large.csv}"
export CISV_BIN="${CISV_BIN:-./cli/build/cisv}"

# Build if needed
if [ ! -f "$CISV_BIN" ]; then
    echo "Building CISV..."
    make -C "$SCRIPTS_DIR/.." all
    echo ""
fi

# Run all tests
tests=(
    "test_simd.sh"
    "test_memory.sh"
    "test_compiler_flags.sh"
    "test_quote_buffer.sh"
    "test_cache.sh"
    "perf_test_suite.sh"
)

for test in "${tests[@]}"; do
    echo ""
    echo "========================================"
    echo "Running: $test"
    echo "========================================"
    echo ""

    test_path="$SCRIPTS_DIR/$test"
    if [ -f "$test_path" ]; then
        bash "$test_path" 2>&1 | tee "$RESULTS_DIR/${test%.sh}.log"
    else
        echo "  (not found, skipping)"
    fi
done

# Generate summary
echo ""
echo "========================================"
echo "Generating summary..."
echo "========================================"

cat > "$RESULTS_DIR/SUMMARY.md" << EOF
# Performance Test Results

## Test Date
$(date)

## System Info
$(uname -a)

## CPU Info
$(grep "model name" /proc/cpuinfo 2>/dev/null | head -1 || echo "N/A")
$(grep "cpu cores" /proc/cpuinfo 2>/dev/null | head -1 || echo "N/A")

## SIMD Support
$(grep -oE 'avx512f|avx2|sse4_2|sse2' /proc/cpuinfo 2>/dev/null | sort -u | tr '\n' ' ' || echo "N/A")

## Test Results
See individual log files for details.

## Files
$(ls -la "$RESULTS_DIR"/*.log 2>/dev/null || echo "No log files")
EOF

echo ""
echo "=============================================="
echo "Results saved to: $RESULTS_DIR/"
echo "=============================================="
