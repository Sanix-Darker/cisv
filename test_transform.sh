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

    echo -e "${YELLOW}Running Valgrind memory analysis...${NC}"
    echo "--------------------------------------------"

    # Run valgrind and save to file to avoid hanging
    valgrind \
        --leak-check=full \
        --show-leak-kinds=definite,indirect \
        --track-origins=yes \
        --suppressions=valgrind-node.supp \
        --log-file=valgrind_output.log \
        node --expose-gc test_transform_leak_test.js 2>&1

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
rm -f leak_test*.csv valgrind_output.log

echo ""
echo "============================================"
echo "Transform tests completed!"
echo "============================================"
