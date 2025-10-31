const { cisvParser } = require('../build/Release/cisv');
const fs = require('fs');
const path = require('path');

// Process large CSV files with progress reporting
function processLargeFile(filePath) {
  const parser = new cisvParser();
  const stats = fs.statSync(filePath);
  const totalSize = stats.size;
  let processed = 0;

  const stream = fs.createReadStream(filePath);
  const startTime = process.hrtime();

  stream.on('data', (chunk) => {
    processed += chunk.length;
    parser.write(chunk);

    // Report progress every MB
    if (processed % (1024 * 1024) === 0) {
      const percent = (processed / totalSize * 100).toFixed(1);
      const [seconds] = process.hrtime(startTime);
      console.log(`Processed: ${percent}% (${seconds}s)`);
    }
  });

  stream.on('end', () => {
    parser.end();
    const rows = parser.getRows();
    const [seconds] = process.hrtime(startTime);
    console.log(`Completed! Processed ${rows.length} rows in ${seconds}s`);
  });
}

processLargeFile(process.argv[2] || path.join(__dirname, '../fixtures/large.csv'));
