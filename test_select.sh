#!/bin/bash

echo -ne "\n\n------------------------------"
echo -ne "\n==== CISV PARSER checks ===\n"
echo -ne "------------------------------\n\n"

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
echo ""
echo "All writer tests completed!"

echo -ne "\n\n------------------------------"
echo -ne "\n==== CISV WRITER checks ===\n"
echo -ne "------------------------------\n\n"

# Build if needed
if [ ! -f "./cisv" ]; then
    make clean
    make cli
fi

echo ""
echo "1. Testing basic generation (10 rows):"
./cisv write -g 10 -o test_basic.csv
cat test_basic.csv
echo ""

echo "2. Testing with custom delimiter (semicolon):"
./cisv write -g 5 -d ';' -o test_delim.csv
cat test_delim.csv
echo ""

echo "3. Testing with always quote:"
./cisv write -g 5 -Q -o test_quoted.csv
cat test_quoted.csv
echo ""

echo "4. Testing CRLF line endings:"
./cisv write -g 3 -r -o test_crlf.csv
file test_crlf.csv
od -c test_crlf.csv | head -2
echo ""

echo "5. Testing benchmark mode (1000 rows):"
./cisv write -g 1000 -o test_bench.csv -b
echo ""

echo "6. Testing memory leaks with valgrind:"
if command -v valgrind > /dev/null 2>&1; then
    valgrind --leak-check=full ./cisv write -g 100 -o test_valgrind.csv
else
    echo "Valgrind not installed"
fi

# Cleanup
rm -f test_*.csv

echo ""
echo "All writer tests completed!"
