#!/bin/bash

set -e

# Output file
OUTPUT_FILE="BENCHMARKS.md"

# Initialize markdown file
echo "## 📊 Benchmark Results" > "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "**Date:** $(date -u +"%Y-%m-%d %H:%M:%S UTC")" >> "$OUTPUT_FILE"
echo "**Commit:** ${GITHUB_SHA:-unknown}" >> "$OUTPUT_FILE"
echo "**Branch:** ${GITHUB_REF_NAME:-unknown}" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "---" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Run JavaScript benchmarks
echo "Running JavaScript benchmarks..."
echo "## JavaScript Library Benchmarks" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

if npm run benchmark-js > benchmark-js-output.txt 2>&1; then
    # Extract sorted tables from output
    sed -n '/### Synchronous Results (sorted by speed/,/^$/p' benchmark-js-output.txt >> "$OUTPUT_FILE" || true
    echo "" >> "$OUTPUT_FILE"
    sed -n '/### Synchronous Results (with data access - sorted by speed)/,/^$/p' benchmark-js-output.txt >> "$OUTPUT_FILE" || true
    echo "" >> "$OUTPUT_FILE"
    sed -n '/### Asynchronous Results (sorted by speed/,/^$/p' benchmark-js-output.txt >> "$OUTPUT_FILE" || true
    echo "" >> "$OUTPUT_FILE"
    sed -n '/### Asynchronous Results (with data access - sorted by speed)/,/^$/p' benchmark-js-output.txt >> "$OUTPUT_FILE" || true
else
    echo "JavaScript benchmarks failed, see benchmark-js-output.txt for details" >> "$OUTPUT_FILE"
fi

# Run CLI benchmarks
echo "Running CLI benchmarks..."
echo "" >> "$OUTPUT_FILE"
echo "## CLI Tool Benchmarks" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

if bash ./benchmark_cli.sh > benchmark-cli-output.txt 2>&1; then
    # Extract sorted results
    sed -n '/Sorted by Speed/,/Sorted by Operations\|^$/p' benchmark-cli-output.txt >> "$OUTPUT_FILE" || true
    echo "" >> "$OUTPUT_FILE"
    sed -n '/Sorted by Operations/,/^===/p' benchmark-cli-output.txt | grep -v "===" >> "$OUTPUT_FILE" || true
else
    echo "CLI benchmarks failed, see benchmark-cli-output.txt for details" >> "$OUTPUT_FILE"
fi

echo "" >> "$OUTPUT_FILE"
echo "---" >> "$OUTPUT_FILE"
echo "_Benchmarks completed at $(date -u +"%Y-%m-%d %H:%M:%S UTC")_" >> "$OUTPUT_FILE"

echo "Benchmarks complete! Results saved to $OUTPUT_FILE"
