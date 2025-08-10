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
parser1.destroy();

// Test 2: Chain of transforms on same field
console.log('\nTest 2: Multiple transforms on same field');
const parser2 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(0, 'trim')
    .transform(0, 'lowercase');  // Multiple transforms on field 0
const rows2 = parser2.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows2.length} rows with chained transforms`);
parser2.destroy();

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
parser3.destroy();

// Test 4: JavaScript callback transforms
console.log('\nTest 4: JavaScript callback transforms');
const parser4 = new cisvParser()
    .transform(0, (val) => val.toUpperCase())
    .transform(1, (val) => val.trim())
    .transform(2, (val) => parseInt(val) * 2);
const rows4 = parser4.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows4.length} rows with JS callbacks`);
parser4.destroy();

// Test 5: Mixed transforms (native + JS)
console.log('\nTest 5: Mixed native and JS transforms');
const parser5 = new cisvParser()
    .transform(0, 'uppercase')
    .transform(1, (val) => val.replace('@', '_at_'))
    .transform(2, 'to_int');
const rows5 = parser5.parseSync('leak_test.csv');
console.log(`  - Parsed ${rows5.length} rows with mixed transforms`);
parser5.destroy();

// Test 6: Multiple parser instances
console.log('\nTest 6: Multiple parser instances');
const parsers = [];
for (let i = 0; i < 10; i++) {
    const p = new cisvParser()
        .transform(0, 'uppercase')
        .transform(1, 'trim');
    parsers.push(p);
    p.parseSync('leak_test.csv');
    p.destroy();
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
parser7.destroy();

// Cleanup
fs.unlinkSync('leak_test.csv');
fs.unlinkSync('leak_test_large.csv');
fs.unlinkSync('leak_test_clean.csv');

console.log('\nAll tests completed successfully!');
