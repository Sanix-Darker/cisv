'use strict';
const { cisvParser } = require('../build/Release/cisv');
const fs = require('fs');
const path = require('path');

const dataFilePath = path.join(__dirname, '../fixtures/data.csv');

// Ensure the data.csv file exists before running
if (!fs.existsSync(dataFilePath)) {
    console.log('Creating a sample data.csv file...');
    const sampleData = `id,name,email,city
1,John Doe,john.doe@email.com,New York
2,Jane Smith,"jane.smith@email.com",Los Angeles
3,Peter Jones,peter.jones@email.com,"San Francisco"
4,Mary Williams,"mary.w@email.com",Chicago`;
    fs.writeFileSync(dataFilePath, sampleData);
    console.log('Sample data.csv created.\n');
}

// Sync Parsing Example (for comparison)
try {
    const syncParser = new cisvParser();
    // const dataFilePath = path.join(__dirname, '../data.csv');
    const rows = syncParser.parseSync(dataFilePath);
    console.log(`Sync parsing successful. Total rows found: ${rows.length}`);
} catch (e) {
    console.error('Sync parsing failed:', e);
}

console.log('\n' + '-'.repeat(40) + '\n');

// Stream Parsing Example ---
console.log('Starting stream parsing...');
const streamParser = new cisvParser();

fs.createReadStream(dataFilePath)
  .on('data', chunk => {
      try {
          streamParser.write(chunk);
      } catch (e) {
          console.error('Error during stream write:', e);
      }
  })
  .on('end', () => {
    console.log('Stream finished.');

    // Finalize the parsing process. This processes any remaining data.
    streamParser.end();

    // ✨ NOW, USE getRows() TO RETRIEVE THE RESULTS ✨
    const allRows = streamParser.getRows();

    console.log(`Total rows from stream: ${allRows.length}`);

    if (allRows.length > 2) {
        // As requested: get a specific line. Let's get the 3rd line (index 2).
        const specificLine = allRows[2];
        console.log('Getting a specific line (line 3):', specificLine);
    } else {
        console.log('Not enough rows to get the 3rd line.');
    }
  })
  .on('error', (err) => {
    console.error('Stream error:', err);
  });
