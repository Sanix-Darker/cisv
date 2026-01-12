#!/bin/bash
# Quote Buffer Stress Test

echo "=== Quote Buffer Stress Test ==="
echo ""

CISV_BIN="${CISV_BIN:-./cli/build/cisv}"

# Create CSV with many quoted fields of varying sizes
echo "Creating stress test file with quoted fields..."
python3 << 'EOF'
import csv
import random
import string

with open('/tmp/quoted_stress.csv', 'w', newline='') as f:
    w = csv.writer(f, quoting=csv.QUOTE_ALL)
    w.writerow(['id', 'small', 'medium', 'large', 'huge'])
    for i in range(100000):
        w.writerow([
            i,
            ''.join(random.choices(string.ascii_letters, k=10)),
            ''.join(random.choices(string.ascii_letters, k=100)),
            ''.join(random.choices(string.ascii_letters, k=1000)),
            ''.join(random.choices(string.ascii_letters + ' "', k=5000))
        ])
EOF

echo "Created stress test file: $(du -h /tmp/quoted_stress.csv)"

echo ""
echo "Running parse benchmark..."
start=$(date +%s.%N)
"$CISV_BIN" parse /tmp/quoted_stress.csv > /dev/null 2>&1
end=$(date +%s.%N)
elapsed=$(echo "$end - $start" | bc)
echo "Parse time: ${elapsed}s"

file_mb=$(du -m /tmp/quoted_stress.csv | cut -f1)
speed=$(echo "scale=2; $file_mb / $elapsed" | bc)
echo "Speed: ${speed} MB/s"

echo ""
echo "Memory usage during parse:"
if command -v /usr/bin/time &> /dev/null; then
    /usr/bin/time -v "$CISV_BIN" parse /tmp/quoted_stress.csv 2>&1 | \
        grep -E "Maximum resident|Minor|Major" || true
fi

# Test with varying quote densities
echo ""
echo "Testing different quote densities..."

for density in 0 25 50 75 100; do
    python3 -c "
import csv
import random
with open('/tmp/quote_density.csv', 'w', newline='') as f:
    w = csv.writer(f)
    for i in range(100000):
        if random.randint(0, 100) < $density:
            w.writerow([i, '\"quoted value\"', 'normal'])
        else:
            w.writerow([i, 'normal value', 'normal'])
"

    start=$(date +%s.%N)
    "$CISV_BIN" parse /tmp/quote_density.csv > /dev/null 2>&1
    end=$(date +%s.%N)
    elapsed=$(echo "$end - $start" | bc)
    printf "  %3d%% quoted: %.4fs\n" "$density" "$elapsed"
done

rm -f /tmp/quoted_stress.csv /tmp/quote_density.csv
echo ""
echo "Done."
