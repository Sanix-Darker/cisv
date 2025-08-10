#!/bin/bash
# Test script for CSV transform functionality with detailed memory leak detection

set -e  # Exit on error

echo "============================================"
echo "CSV Transform Functionality & Memory Testing"
echo "============================================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Build if needed
if [ ! -f "./build/Release/cisv.node" ]; then
    echo -e "${YELLOW}Building Node.js addon...${NC}"
    make build
fi

# Create test script
echo -e "\n${GREEN}Running transform tests...${NC}"
node ./examples/transform.js

# Test memory leaks if valgrind is available
if command -v valgrind > /dev/null 2>&1; then
    echo ""
    echo "============================================"
    echo "MEMORY LEAK DETECTION TESTS"
    echo "============================================"

    # Create comprehensive test file
    cat > test_transform_leak.js << 'EOF'
const { cisvParser } = require('./build/Release/cisv');
const fs = require('fs');

console.log('Starting memory leak tests...\n');

// Test 1: Basic transforms
console.log('Test 1: Basic transforms');
fs.writeFileSync('leak_test.csv', 'name,email,age\njohn,john@test.com,25\njane,jane@test.com,30');
const parser1 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(1, 'lowercase')
    .transform(2, 'trim');
const rows1 = parser1.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows1.length} rows with 3 transforms`);

// Test 2: Chain of transforms on same field
console.log('\nTest 2: Multiple transforms on same field');
const parser2 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(0, 'trim')
    .transform(0, 'lowercase');  // Multiple transforms on field 0
const rows2 = parser2.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows2.length} rows with chained transforms`);

// Test 3: Large dataset with transforms
console.log('\nTest 3: Large dataset (1000 rows)');
let largeCSV = 'col1,col2,col3,col4,col5\n';
for (let i = 0; i < 1000; i++) {
    largeCSV += `value${i},  data${i}  ,${i},test${i}@email.com,  trimme  \n`;
}
fs.writeFileSync('leak_test_large.csv', largeCSV);

const parser3 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(1, 'trim')
    .transform(2, 'to_int')
    .transform(3, 'lowercase')
    .transform(4, 'trim');
const rows3 = parser3.parseSync('leak_test_large.csv');
console.log(`  - Parsed ${rows3.length} rows with 5 transforms`);

// Test 4: JavaScript callback transforms
console.log('\nTest 4: JavaScript callback transforms');
const parser4 = new cisvParser()
    .transform(0, (val) => val.toUpperCase())
    .transform(1, (val) => val.trim())
    .transform(2, (val) => parseInt(val) * 2);
const rows4 = parser4.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows4.length} rows with JS callbacks`);

// Test 5: Mixed transforms (native + JS)
console.log('\nTest 5: Mixed native and JS transforms');
const parser5 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(1, (val) => val.replace('@', '_at_'))
    .transform(2, 'to_int');
const rows5 = parser5.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows5.length} rows with mixed transforms`);

// Test 6: Multiple parser instances
console.log('\nTest 6: Multiple parser instances');
const parsers = [];
for (let i = 0; i < 10; i++) {
    const p = new cisvParser()
        .transform(0, 'uppercase')
        .transform(1, 'trim');
    parsers.push(p);
    p.parseSync('leak_test.csv');
}
console.log(`  - Created and used ${parsers.length} parser instances`);

// Test 7: Transform with no actual changes (edge case)
console.log('\nTest 7: Transform with no changes needed');
fs.writeFileSync('leak_test_clean.csv', 'ALREADY_UPPER,already_lower,123\n');
const parser7 = new cisvParser()
    .transform(0, 'uppercase')  // Already uppercase
    .transform(1, 'lowercase')  // Already lowercase
    .transform(2, 'to_int');     // Already a number
const rows7 = parser7.parseSync('leak_test_clean.csv');
console.log(`  - Parsed ${rows7.length} rows (no-op transforms)`);

// Cleanup
fs.unlinkSync('leak_test.csv');
fs.unlinkSync('leak_test_large.csv');
fs.unlinkSync('leak_test_clean.csv');

console.log('\nAll tests completed successfully!');
process.exit(0);
EOF

    echo -e "${YELLOW}Running Valgrind memory analysis...${NC}"
    echo "--------------------------------------------"

    # Run valgrind and save to file to avoid hanging
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --log-file=valgrind_output.log \
        --error-exitcode=1 \
        node test_transform_leak.js 2>&1

    VALGRIND_EXIT_CODE=$?

    # Read the valgrind output from file
    if [ -f valgrind_output.log ]; then
        VALGRIND_OUTPUT=$(cat valgrind_output.log)

        # Parse and display results
        echo "$VALGRIND_OUTPUT" | grep -A5 "HEAP SUMMARY" || echo "No HEAP SUMMARY found"
        echo "--------------------------------------------"

        # Extract key metrics
        DEFINITELY_LOST=$(echo "$VALGRIND_OUTPUT" | grep "definitely lost:" | tail -1)
        INDIRECTLY_LOST=$(echo "$VALGRIND_OUTPUT" | grep "indirectly lost:" | tail -1)
        POSSIBLY_LOST=$(echo "$VALGRIND_OUTPUT" | grep "possibly lost:" | tail -1)
        STILL_REACHABLE=$(echo "$VALGRIND_OUTPUT" | grep "still reachable:" | tail -1)
        ERROR_SUMMARY=$(echo "$VALGRIND_OUTPUT" | grep "ERROR SUMMARY:" | tail -1)

        echo -e "\n${YELLOW}MEMORY LEAK SUMMARY:${NC}"
        echo "--------------------------------------------"

        # Check for definitely lost bytes
        if [ -n "$DEFINITELY_LOST" ]; then
            if echo "$DEFINITELY_LOST" | grep -q "0 bytes in 0 blocks"; then
                echo -e "${GREEN}✓ Definitely lost: 0 bytes${NC}"
                DEFINITELY_OK=true
            else
                echo -e "${RED}✗ $DEFINITELY_LOST${NC}"
                DEFINITELY_OK=false
            fi
        else
            echo -e "${YELLOW}⚠ Definitely lost: not found in output${NC}"
            DEFINITELY_OK=false
        fi

        # Check for indirectly lost bytes
        if [ -n "$INDIRECTLY_LOST" ]; then
            if echo "$INDIRECTLY_LOST" | grep -q "0 bytes in 0 blocks"; then
                echo -e "${GREEN}✓ Indirectly lost: 0 bytes${NC}"
            else
                echo -e "${YELLOW}⚠ $INDIRECTLY_LOST${NC}"
            fi
        fi

        # Check for possibly lost bytes
        if [ -n "$POSSIBLY_LOST" ]; then
            if echo "$POSSIBLY_LOST" | grep -q "0 bytes in 0 blocks"; then
                echo -e "${GREEN}✓ Possibly lost: 0 bytes${NC}"
            else
                echo -e "${YELLOW}⚠ $POSSIBLY_LOST${NC}"
            fi
        fi

        # Display still reachable (usually OK for Node.js)
        if [ -n "$STILL_REACHABLE" ]; then
            echo -e "  $STILL_REACHABLE"
        fi

        # Check error summary
        echo "--------------------------------------------"
        if [ -n "$ERROR_SUMMARY" ]; then
            if echo "$ERROR_SUMMARY" | grep -q "0 errors from 0 contexts"; then
                echo -e "${GREEN}✓ $ERROR_SUMMARY${NC}"
                ERROR_OK=true
            else
                echo -e "${RED}✗ $ERROR_SUMMARY${NC}"
                ERROR_OK=false
            fi
        else
            echo -e "${YELLOW}⚠ ERROR SUMMARY not found${NC}"
            ERROR_OK=false
        fi

        # Determine overall pass/fail
        if [ "$DEFINITELY_OK" = true ] && [ "$ERROR_OK" = true ]; then
            echo -e "${GREEN}✓ No memory leaks detected!${NC}"
            MEMORY_TEST_PASSED=true
        else
            echo -e "${RED}✗ Memory issues detected!${NC}"
            MEMORY_TEST_PASSED=false

            # Show detailed errors if any
            echo -e "\n${YELLOW}Checking for detailed errors...${NC}"
            grep -i "invalid\|definitely lost\|indirectly lost" valgrind_output.log | head -20 || echo "No specific errors found"
        fi

        # Save report
        mv valgrind_output.log valgrind_report.txt
        echo -e "\n${YELLOW}Full Valgrind report saved to: valgrind_report.txt${NC}"
    else
        echo -e "${RED}Error: Valgrind output file not created${NC}"
        MEMORY_TEST_PASSED=false
    fi

    # Cleanup
    rm -f test_transform_leak.js

    # Exit with appropriate code
    if [ "$MEMORY_TEST_PASSED" = true ]; then
        echo -e "\n${GREEN}============================================${NC}"
        echo -e "${GREEN}ALL MEMORY TESTS PASSED SUCCESSFULLY!${NC}"
        echo -e "${GREEN}============================================${NC}"
    else
        echo -e "\n${RED}============================================${NC}"
        echo -e "${RED}MEMORY TESTS FAILED - LEAKS DETECTED${NC}"
        echo -e "${RED}============================================${NC}"
        echo -e "\n${YELLOW}Debug tips:${NC}"
        echo "1. Check valgrind_report.txt for full details"
        echo "2. Look for 'definitely lost' blocks in the report"
        echo "3. Use gdb to trace the allocation points"
        echo "4. Consider using AddressSanitizer for additional debugging"
    fi
else
    echo -e "\n${YELLOW}Warning: Valgrind not installed. Skipping memory leak tests.${NC}"
    echo "To install valgrind:"
    echo "  Ubuntu/Debian: sudo apt-get install valgrind"
    echo "  macOS: brew install valgrind"
    echo "  RHEL/CentOS: sudo yum install valgrind"
fi

# Cleanup any remaining files
rm -f test_transform_basic.js leak_test*.csv valgrind_output.log

echo ""
echo "============================================"
echo "Transform tests completed!"
echo "============================================"
