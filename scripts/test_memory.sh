#!/bin/bash
# Memory Allocation Analysis

echo "=== Memory Allocation Analysis ==="
echo ""

LARGE_CSV="${LARGE_CSV:-large.csv}"
CISV_BIN="${CISV_BIN:-./cli/build/cisv}"

# Create test file if needed
if [ ! -f "$LARGE_CSV" ]; then
    echo "Creating test file (100k rows)..."
    seq 1 100000 | awk '{print $1",field"$1",value"$1}' > "$LARGE_CSV"
fi

echo "Test file: $(du -h $LARGE_CSV)"
echo ""

if ! command -v valgrind &> /dev/null; then
    echo "valgrind not available, skipping detailed analysis"
    echo ""
    echo "Basic memory usage (via /usr/bin/time):"
    if command -v /usr/bin/time &> /dev/null; then
        /usr/bin/time -v "$CISV_BIN" parse "$LARGE_CSV" 2>&1 | grep -E "Maximum resident|Minor|Major" || true
    fi
    exit 0
fi

# Track allocations with valgrind
echo "Tracking memory allocations..."
valgrind --tool=memcheck --track-origins=yes \
    --log-file=/tmp/memcheck.log \
    "$CISV_BIN" parse "$LARGE_CSV" > /dev/null 2>&1

echo ""
echo "Allocation summary:"
grep -E "total heap usage|definitely lost|indirectly lost|possibly lost" /tmp/memcheck.log || true

# Check for memory leaks
leaks=$(grep "definitely lost" /tmp/memcheck.log | grep -oE '[0-9,]+ bytes' | head -1)
if [ -n "$leaks" ] && [ "$leaks" != "0 bytes" ]; then
    echo ""
    echo "WARNING: Memory leaks detected: $leaks"
fi

# Massif for memory over time
echo ""
echo "Running massif for memory timeline..."
valgrind --tool=massif --time-unit=B --massif-out-file=/tmp/massif.out \
    "$CISV_BIN" parse "$LARGE_CSV" > /dev/null 2>&1

if command -v ms_print &> /dev/null; then
    echo ""
    echo "Memory usage over time (first 30 lines):"
    ms_print /tmp/massif.out 2>/dev/null | head -30
fi

# Peak memory
peak=$(grep "mem_heap_B" /tmp/massif.out 2>/dev/null | cut -d'=' -f2 | sort -n | tail -1)
if [ -n "$peak" ]; then
    peak_mb=$(echo "scale=2; $peak / 1024 / 1024" | bc)
    echo ""
    echo "Peak heap memory: ${peak_mb} MB"
fi

rm -f /tmp/memcheck.log /tmp/massif.out
echo ""
echo "Done."
