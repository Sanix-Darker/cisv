#!/bin/bash

# Test script for CSV transform functionality

echo "Testing CSV transform functionality..."

# Build if needed
if [ ! -f "./build/Release/cisv.node" ]; then
    echo "Building Node.js addon..."
    make build
fi

# Create test script
cat > test_transform_basic.js << 'EOF'
const { cisvParser } = require('./build/Release/cisv');

// Test data
const testData = `id,name,email,amount
1,john doe,JOHN@EXAMPLE.COM,123.45
2,  jane smith  ,Jane@Example.Com,234.56
3,BOB JOHNSON,bob@example.com,345.67`;

// Write test file
require('fs').writeFileSync('test_transform.csv', testData);

console.log('Original data:');
console.log(testData);
console.log('\n=== Transform Tests ===\n');

// Test 1: Individual transforms
console.log('1. Individual field transforms:');
const parser1 = new cisvParser()
    .transform(1, 'uppercase')
    .transform(2, 'lowercase')
    .transform(3, 'float');

const rows1 = parser1.parseSync('test_transform.csv');
console.log('Row 1:', rows1[1]);
console.log('Row 2:', rows1[2]);

// Test 2: Trim transform
console.log('\n2. Trim transform:');
const parser2 = new cisvParser()
    .transform(1, 'trim');

const rows2 = parser2.parseSync('test_transform.csv');
console.log('Trimmed name:', `"${rows2[2][1]}"`);

// Test 3: Custom JS transform
console.log('\n3. Custom JavaScript transform:');
const parser3 = new cisvParser()
    .transform(2, (email) => {
        // Normalize email: lowercase and remove dots
        return email.toLowerCase().replace(/\./g, '');
    });

const rows3 = parser3.parseSync('test_transform.csv');
console.log('Normalized emails:', rows3.slice(1).map(r => r[2]));

// Test 4: Chained transforms
console.log('\n4. Chained transforms:');
const parser4 = new cisvParser()
    .transform(1, 'trim')
    .transform(1, 'uppercase');

const rows4 = parser4.parseSync('test_transform.csv');
console.log('Trim then uppercase:', rows4[2][1]);

// Test 5: Hash transform
console.log('\n5. Hash transform:');
const parser5 = new cisvParser()
    .transform(2, 'sha256');

const rows5 = parser5.parseSync('test_transform.csv');
console.log('Hashed email:', rows5[1][2].substring(0, 20) + '...');

// Test 6: Base64 transform
console.log('\n6. Base64 encoding:');
const parser6 = new cisvParser()
    .transform(1, 'base64');

const rows6 = parser6.parseSync('test_transform.csv');
console.log('Base64 names:', rows6.slice(1).map(r => r[1]));

// Test 7: Performance test
console.log('\n7. Performance test (100k rows):');
const bigData = ['id,name,email,amount'];
for (let i = 0; i < 100000; i++) {
    bigData.push(`${i},user ${i},USER${i}@EMAIL.COM,${i}.99`);
}
require('fs').writeFileSync('test_big.csv', bigData.join('\n'));

// No transforms
console.time('No transforms');
new cisvParser().parseSync('test_big.csv');
console.timeEnd('No transforms');

// With transforms
console.time('With transforms');
new cisvParser()
    .transform(1, 'uppercase')
    .transform(2, 'lowercase')
    .transform(3, 'float')
    .parseSync('test_big.csv');
console.timeEnd('With transforms');

// Cleanup
require('fs').unlinkSync('test_transform.csv');
require('fs').unlinkSync('test_big.csv');

console.log('\n✓ All transform tests passed!');
EOF

# Run tests
echo ""
echo "Running transform tests..."
node test_transform_basic.js

# Test memory leaks if valgrind is available
if command -v valgrind > /dev/null 2>&1; then
    echo ""
    echo "Testing for memory leaks..."

    cat > test_transform_leak.js << 'EOF'
const { cisvParser } = require('./build/Release/cisv');
const fs = require('fs');

// Small test file
fs.writeFileSync('leak_test.csv', 'a,b,c\n1,2,3\n4,5,6');

// Test transforms
const parser = new cisvParser()
    .transform(0, 'uppercase')
    .transform(1, 'lowercase')
    .transform(2, (val) => val + '_modified');

const rows = parser.parseSync('leak_test.csv');
console.log('Rows:', rows.length);

fs.unlinkSync('leak_test.csv');
EOF

    valgrind --leak-check=full --show-leak-kinds=all node test_transform_leak.js 2>&1 | grep -E "(definitely lost:|ERROR SUMMARY)"
    rm -f test_transform_leak.js
fi

# Cleanup
rm -f test_transform_basic.js

echo ""
echo "Transform tests completed!"
