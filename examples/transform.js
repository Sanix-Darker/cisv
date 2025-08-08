const { cisvParser, TransformType } = require('../build/Release/cisv');
const fs = require('fs');
const path = require('path');

// Create test CSV if it doesn't exist
const testFile = path.join(__dirname, 'transform_test.csv');
if (!fs.existsSync(testFile)) {
    const data = `id,name,email,amount,date
1,john doe,JOHN@EXAMPLE.COM,1234.56,2024-01-01
2,  jane smith  ,jane@EXAMPLE.com,2345.67,2024-01-02
3,bob johnson,BOB@EXAMPLE.COM,3456.78,2024-01-03`;
    fs.writeFileSync(testFile, data);
}

console.log('=== Native C Transform Examples ===\n');
console.log('Available transform types:', TransformType);
console.log('');

// Example 1: Basic transforms using native C code
console.log('1. Native C transforms:');
const parser1 = new cisvParser();
parser1.transform(1, 'uppercase')     // Name to uppercase (C implementation)
       .transform(2, 'lowercase')     // Email to lowercase (C implementation)
       .transform(3, 'to_float');     // Amount to float (C implementation)

const rows1 = parser1.parseSync(testFile);
console.log('Transformed with C code:', rows1[1]);

// Example 2: Trim transform (C implementation)
console.log('\n2. Native trim:');
const parser2 = new cisvParser();
parser2.transform(1, 'trim');

const rows2 = parser2.parseSync(testFile);
console.log('Original: "  jane smith  "');
console.log('Trimmed with C:', `"${rows2[2][1]}"`);

// Example 3: SHA256 hashing (C implementation)
console.log('\n3. Native SHA256:');
const parser3 = new cisvParser();
parser3.transform(2, 'hash_sha256');

const rows3 = parser3.parseSync(testFile);
console.log('Hashed email (C implementation):', rows3[1][2]);

// Example 4: Base64 encoding (C implementation)
console.log('\n4. Native Base64:');
const parser4 = new cisvParser();
parser4.transform(1, 'base64_encode');

const rows4 = parser4.parseSync(testFile);
console.log('Base64 encoded names (C):', rows4.slice(1).map(row => row[1]));

// Example 5: Multiple transforms on different fields
console.log('\n5. Multiple native transforms:');
const parser5 = new cisvParser();
parser5.transform(0, 'to_int')        // ID to int
       .transform(1, 'trim')           // Trim name
       .transform(1, 'uppercase')      // Then uppercase
       .transform(2, 'lowercase')      // Email lowercase
       .transform(3, 'to_float');      // Amount to float

const rows5 = parser5.parseSync(testFile);
console.log('Multiple transforms:', rows5[1]);

// Example 6: Performance test with native transforms
console.log('\n6. Performance test (native C transforms):');

// Generate larger test file
const largeCsv = 'large_native_test.csv';
const rowCount = 100000;
console.log(`Generating ${rowCount} rows...`);

let csvContent = 'id,name,email,amount\n';
for (let i = 0; i < rowCount; i++) {
    csvContent += `${i},  user ${i}  ,USER${i}@EXAMPLE.COM,${i * 1.23}\n`;
}
fs.writeFileSync(largeCsv, csvContent);

// Test without transforms
console.time('No transforms');
const parserNoTransform = new cisvParser();
parserNoTransform.parseSync(largeCsv);
console.timeEnd('No transforms');

// Test with C transforms
console.time('With C transforms');
const parserWithTransform = new cisvParser();
parserWithTransform.transform(1, 'trim')
                   .transform(1, 'uppercase')
                   .transform(2, 'lowercase')
                   .transform(3, 'to_float');
parserWithTransform.parseSync(largeCsv);
console.timeEnd('With C transforms');

// Get stats
const stats = parserWithTransform.getStats();
console.log('Parse stats:', stats);

// Example 7: Parse string content
console.log('\n7. Parse string with transforms:');
const csvString = `name,value
test one,123
TEST TWO,456`;

const parser7 = new cisvParser();
parser7.transform(0, 'uppercase')
       .transform(1, 'to_int');

const rows7 = parser7.parseString(csvString);
console.log('Parsed string:', rows7);

// Example 8: Streaming with transforms
console.log('\n8. Streaming with transforms:');
const parser8 = new cisvParser();
parser8.transform(0, 'uppercase');

parser8.write('name,value\n');
parser8.write('test,123\n');
parser8.write('another,456\n');
parser8.end();

const rows8 = parser8.getRows();
console.log('Streamed rows:', rows8);

// Example 9: Count rows (static method)
console.log('\n9. Count rows:');
const rowCount2 = cisvParser.countRows(testFile);
console.log(`File has ${rowCount2} rows`);

// Example 10: Clear and reuse parser
console.log('\n10. Reuse parser:');
const parser10 = new cisvParser();
parser10.transform(0, 'uppercase');

// First parse
const firstParse = parser10.parseSync(testFile);
console.log('First parse rows:', firstParse.length);

// Clear and parse again with different transforms
parser10.clear();
parser10.clearTransforms();
parser10.transform(1, 'lowercase');

const secondParse = parser10.parseSync(testFile);
console.log('Second parse rows:', secondParse.length);

// Cleanup
fs.unlinkSync(largeCsv);

console.log('\n=== Native Transform Examples Complete ===');
console.log('All transforms executed using optimized C code!');
