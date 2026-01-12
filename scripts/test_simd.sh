#!/bin/bash
# SIMD Feature Detection and Testing

echo "=== SIMD Feature Detection ==="
echo ""

# Check CPU features
echo "CPU SIMD Support:"
if [ -f /proc/cpuinfo ]; then
    grep -oE 'avx512f|avx512bw|avx2|sse4_2|sse2|neon' /proc/cpuinfo 2>/dev/null | sort -u | tr '\n' ' '
    echo ""
else
    echo "  (Cannot read /proc/cpuinfo)"
fi

echo ""

# Check compiled binary
CISV_BIN="${CISV_BIN:-./cli/build/cisv}"
if [ -f "$CISV_BIN" ]; then
    echo "Compiled SIMD instructions in binary:"

    avx512_count=$(objdump -d "$CISV_BIN" 2>/dev/null | grep -cE 'vmov.*zmm|vpcmpeq.*zmm' || echo "0")
    avx2_count=$(objdump -d "$CISV_BIN" 2>/dev/null | grep -cE 'vmovdqu|vpcmpeqb|vpor' || echo "0")
    sse_count=$(objdump -d "$CISV_BIN" 2>/dev/null | grep -cE 'movdqa|pcmpeqb|por' || echo "0")

    echo "  AVX-512 instructions: $avx512_count"
    echo "  AVX2 instructions: $avx2_count"
    echo "  SSE instructions: $sse_count"
else
    echo "Binary not found at $CISV_BIN"
fi

echo ""

# Test parsing with different field types
echo "Testing SIMD paths..."

# Create test CSV
cat > /tmp/simd_test.csv << 'EOF'
id,name,value,description
1,"test field",100,"A simple description"
2,"another \"quoted\" field",200,"Contains special, characters"
3,simple,300,no quotes needed
4,"field with
newline",400,"multi-line value"
5,"",500,"empty quoted field"
EOF

if [ -f "$CISV_BIN" ]; then
    echo "Parsing test file..."
    "$CISV_BIN" parse /tmp/simd_test.csv > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "  Parse: PASS"
    else
        echo "  Parse: FAIL"
    fi

    count=$("$CISV_BIN" count /tmp/simd_test.csv 2>/dev/null | grep -oE '[0-9]+' | head -1)
    if [ "$count" = "5" ]; then
        echo "  Count (expected 5): PASS ($count)"
    else
        echo "  Count (expected 5): FAIL (got $count)"
    fi
fi

# Run with perf if available
if command -v perf &> /dev/null && [ -f "$CISV_BIN" ]; then
    echo ""
    echo "Performance counters (perf stat):"
    perf stat -e instructions,cycles,cache-misses \
        "$CISV_BIN" parse /tmp/simd_test.csv 2>&1 | tail -10
fi

rm -f /tmp/simd_test.csv
echo ""
echo "Done."
