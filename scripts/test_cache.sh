#!/bin/bash
# Cache Performance Analysis

echo "=== Cache Performance Analysis ==="
echo ""

LARGE_CSV="${LARGE_CSV:-large.csv}"
CISV_BIN="${CISV_BIN:-./cli/build/cisv}"

if ! command -v perf &> /dev/null; then
    echo "perf not available, skipping cache analysis"
    exit 0
fi

# Create test file if needed
if [ ! -f "$LARGE_CSV" ]; then
    echo "Creating test file..."
    seq 1 500000 | awk '{print $1",name"$1",value"$1}' > "$LARGE_CSV"
fi

echo "Test file: $(du -h $LARGE_CSV)"
echo ""

echo "Cache hit/miss analysis:"
perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
    "$CISV_BIN" parse "$LARGE_CSV" 2>&1 | grep -E "cache|LLC|L1" | sed 's/^/  /'

echo ""
echo "Branch prediction:"
perf stat -e branches,branch-misses \
    "$CISV_BIN" parse "$LARGE_CSV" 2>&1 | grep -E "branch" | sed 's/^/  /'

echo ""
echo "Overall performance:"
perf stat -e instructions,cycles,task-clock \
    "$CISV_BIN" parse "$LARGE_CSV" 2>&1 | grep -E "instructions|cycles|task-clock" | sed 's/^/  /'

# Calculate IPC
echo ""
echo "Instructions per cycle (IPC):"
perf stat -e instructions,cycles "$CISV_BIN" parse "$LARGE_CSV" 2>&1 | \
    awk '/instructions/ {inst=$1} /cycles/ {cyc=$1} END {
        gsub(",","",inst); gsub(",","",cyc);
        if (cyc > 0) printf "  IPC: %.2f\n", inst/cyc
    }'

echo ""
echo "Done."
