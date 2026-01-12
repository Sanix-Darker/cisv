#!/bin/bash
# Compiler Flag Comparison Test

echo "=== Compiler Flag Comparison ==="
echo ""

LARGE_CSV="${LARGE_CSV:-large.csv}"
SRC="core/src/parser.c core/src/transformer.c core/src/writer.c"
INC="-Icore/include"
ITERATIONS=3

# Create test file if needed
if [ ! -f "$LARGE_CSV" ]; then
    echo "Creating test file (500k rows)..."
    seq 1 500000 | awk '{print $1",name"$1",value"$1}' > "$LARGE_CSV"
fi

echo "Test file: $(du -h $LARGE_CSV)"
echo ""

run_benchmark() {
    local name="$1"
    local flags="$2"
    local total=0

    printf "Building %-20s: " "$name"

    # Build
    if ! gcc $flags $INC $SRC cli/src/main.c -o /tmp/cisv_test 2>/dev/null; then
        echo "BUILD FAILED"
        return
    fi

    # Run benchmark
    for i in $(seq 1 $ITERATIONS); do
        start=$(date +%s.%N)
        /tmp/cisv_test parse "$LARGE_CSV" > /dev/null 2>&1
        end=$(date +%s.%N)
        elapsed=$(echo "$end - $start" | bc)
        total=$(echo "$total + $elapsed" | bc)
    done

    avg=$(echo "scale=4; $total / $ITERATIONS" | bc)
    file_mb=$(du -m "$LARGE_CSV" | cut -f1)
    speed=$(echo "scale=0; $file_mb / $avg" | bc)

    printf "%.4fs avg (%d MB/s)\n" "$avg" "$speed"
}

echo "Benchmarking different compiler flag combinations..."
echo "(Each test runs $ITERATIONS times, showing average)"
echo ""

run_benchmark "baseline_O2" "-O2 -std=c11"
run_benchmark "O3_only" "-O3 -std=c11"
run_benchmark "O3_native" "-O3 -march=native -mtune=native -std=c11"
run_benchmark "O3_lto" "-O3 -flto -std=c11"
run_benchmark "O3_fastmath" "-O3 -ffast-math -std=c11"
run_benchmark "O3_unroll" "-O3 -funroll-loops -std=c11"
run_benchmark "O3_all" "-O3 -march=native -mtune=native -flto -ffast-math -funroll-loops -fomit-frame-pointer -std=c11"

# Test AVX-512 if supported
if grep -q avx512f /proc/cpuinfo 2>/dev/null; then
    echo ""
    echo "AVX-512 detected, testing..."
    run_benchmark "O3_avx512" "-O3 -march=native -mavx512f -mavx512bw -flto -std=c11"
fi

rm -f /tmp/cisv_test
echo ""
echo "Done."
