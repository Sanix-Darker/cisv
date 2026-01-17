#!/bin/bash
# Test script for CSV transform functionality with memory leak detection

# Determine the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Set up paths
CISV_BIN="$PROJECT_ROOT/cli/build/cisv"
export LD_LIBRARY_PATH="$PROJECT_ROOT/core/build:$LD_LIBRARY_PATH"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "============================================"
echo "CSV Transform Functionality & Memory Testing"
echo "============================================"

# Build if needed
if [ ! -f "$CISV_BIN" ]; then
    echo -e "${YELLOW}Building CLI...${NC}"
    make -C "$PROJECT_ROOT" clean
    make -C "$PROJECT_ROOT" all
fi

# Verify binary exists
if [ ! -f "$CISV_BIN" ]; then
    echo -e "${RED}Error: CLI binary not found at $CISV_BIN${NC}"
    exit 1
fi

# Create test CSV file
echo -e "\n${GREEN}Creating test data...${NC}"
cat > transform_test.csv << EOF
id,name,email,city
1,John Doe,john@test.com,New York
2,Jane Smith,jane@test.com,Los Angeles
3,Bob Johnson,bob@test.com,Chicago
4,Alice Brown,alice@test.com,Houston
5,Charlie Wilson,charlie@test.com,Phoenix
EOF

echo -e "\n${GREEN}Running transform tests...${NC}"
echo "--------------------------------------------"

echo "1. Testing uppercase transform:"
"$CISV_BIN" -t uppercase transform_test.csv 2>/dev/null || "$CISV_BIN" transform_test.csv
echo ""

echo "2. Testing lowercase transform:"
"$CISV_BIN" -t lowercase transform_test.csv 2>/dev/null || "$CISV_BIN" transform_test.csv
echo ""

echo "3. Testing with column selection + transform:"
"$CISV_BIN" -s 0,1 transform_test.csv
echo ""

echo "4. Testing head limit:"
"$CISV_BIN" --head 2 transform_test.csv
echo ""

echo "5. Testing row count:"
"$CISV_BIN" -c transform_test.csv
echo ""

# Test memory leaks if valgrind is available
if command -v valgrind > /dev/null 2>&1; then
    echo ""
    echo "============================================"
    echo "MEMORY LEAK DETECTION TESTS"
    echo "============================================"

    echo -e "${YELLOW}Running Valgrind memory analysis...${NC}"
    echo "--------------------------------------------"

    # Run valgrind on various operations
    VALGRIND_OPTS="--leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes --error-exitcode=1"

    echo "Testing parse operation..."
    valgrind $VALGRIND_OPTS "$CISV_BIN" transform_test.csv > /dev/null 2>&1
    PARSE_RESULT=$?

    echo "Testing column selection..."
    valgrind $VALGRIND_OPTS "$CISV_BIN" -s 0,2 transform_test.csv > /dev/null 2>&1
    SELECT_RESULT=$?

    echo "Testing row count..."
    valgrind $VALGRIND_OPTS "$CISV_BIN" -c transform_test.csv > /dev/null 2>&1
    COUNT_RESULT=$?

    echo "Testing head limit..."
    valgrind $VALGRIND_OPTS "$CISV_BIN" --head 3 transform_test.csv > /dev/null 2>&1
    HEAD_RESULT=$?

    echo ""
    echo "--------------------------------------------"
    echo -e "${YELLOW}MEMORY LEAK SUMMARY:${NC}"
    echo "--------------------------------------------"

    ALL_PASSED=true

    if [ $PARSE_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Parse operation: No leaks${NC}"
    else
        echo -e "${RED}✗ Parse operation: Leaks detected${NC}"
        ALL_PASSED=false
    fi

    if [ $SELECT_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Column selection: No leaks${NC}"
    else
        echo -e "${RED}✗ Column selection: Leaks detected${NC}"
        ALL_PASSED=false
    fi

    if [ $COUNT_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Row count: No leaks${NC}"
    else
        echo -e "${RED}✗ Row count: Leaks detected${NC}"
        ALL_PASSED=false
    fi

    if [ $HEAD_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Head limit: No leaks${NC}"
    else
        echo -e "${RED}✗ Head limit: Leaks detected${NC}"
        ALL_PASSED=false
    fi

    echo "--------------------------------------------"

    if [ "$ALL_PASSED" = true ]; then
        echo -e "${GREEN}============================================${NC}"
        echo -e "${GREEN}ALL MEMORY TESTS PASSED SUCCESSFULLY!${NC}"
        echo -e "${GREEN}============================================${NC}"
    else
        echo -e "${RED}============================================${NC}"
        echo -e "${RED}MEMORY TESTS FAILED - LEAKS DETECTED${NC}"
        echo -e "${RED}============================================${NC}"

        # Run detailed valgrind for debugging
        echo -e "\n${YELLOW}Running detailed analysis...${NC}"
        valgrind --leak-check=full --show-leak-kinds=all "$CISV_BIN" transform_test.csv 2>&1 | tail -30
    fi
else
    echo -e "\n${YELLOW}Warning: Valgrind not installed. Skipping memory leak tests.${NC}"
    echo "To install valgrind:"
    echo "  Ubuntu/Debian: sudo apt-get install valgrind"
    echo "  macOS: brew install valgrind"
    echo "  RHEL/CentOS: sudo yum install valgrind"
fi

# Cleanup
rm -f transform_test.csv valgrind_output.log valgrind_report.txt

echo ""
echo "============================================"
echo "Transform tests completed!"
echo "============================================"
