#!/bin/bash

# Test script to debug column selection

echo "Creating test CSV file..."
cat > test_select.csv << EOF
id,name,email,city
1,John Doe,john@test.com,New York
2,Jane Smith,jane@test.com,Los Angeles
3,Bob Johnson,bob@test.com,Chicago
EOF

echo "Testing column selection..."
echo ""

echo "1. Display all columns (no selection):"
./cisv test_select.csv
echo ""

echo "2. Select column 0 only:"
./cisv -s 0 test_select.csv
echo ""

echo "3. Select columns 0,2:"
./cisv -s 0,2 test_select.csv
echo ""

echo "4. Select columns 0,2,3:"
./cisv -s 0,2,3 test_select.csv
echo ""

echo "5. Test with valgrind (if available):"
if command -v valgrind > /dev/null 2>&1; then
    valgrind --leak-check=full ./cisv -s 0,2,3 test_select.csv
else
    echo "Valgrind not installed"
fi

rm -f test_select.csv
