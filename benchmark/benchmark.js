'use strict';
// direct node ./benchmark/benchmark.js
// means you have to call from :../build/Release/cisv
const { cisvParser } = require('../../build/Release/cisv');
const { parse: csvParseSync } = require('csv-parse/sync');
const { parse: csvParseStream } = require('csv-parse');
const Papa = require('papaparse');
const fastCsv = require('fast-csv');
const fs = require('fs');
const { Suite } = require('benchmark');
const stream = require('stream');
const path = require('path');

// Initial file path from arguments or default to './fixtures/data.csv'
let benchFilePath = process.argv[2] || './fixtures/data.csv';

try {
  // Resolve to an absolute path
  if (!fs.existsSync(benchFilePath)) {
    // If the initial path doesn't exist, try '../fixtures/data.csv'
    const alternativePath = '../fixtures/data.csv';
    if (fs.existsSync(alternativePath)) {
      benchFilePath = alternativePath;
    } else {
      // If none of the files exist, throw an error
      throw new Error("File not found");
    }
  }
} catch (error) {
  console.error("Failed to locate benchmark file in either specified path or '../fixtures':", error);
  throw error;
}

const BENCH_FILE = path.resolve(benchFilePath);
console.log(">>> Using benchmark file:", BENCH_FILE);

// We set a row to retrieve.
// This ensures all parsers do the work to make data accessible.
const TARGET_ROW_INDEX = 4;

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));
const results = { sync: [], sync_data: [], async: [], async_data: [] };

async function runAllBenchmarks() {
    const neatCsv = (await import('neat-csv')).default;

    console.log(`Starting benchmark with file: ${BENCH_FILE}`);
    console.log(`All tests will retrieve row index: ${TARGET_ROW_INDEX}\n`);

    // Prepare and Verify Data
    let fileBuffer;
    try {
        fileBuffer = fs.readFileSync(BENCH_FILE);
    } catch (e) {
        console.error(`Error: Could not read the file "${BENCH_FILE}".`);
        process.exit(1);
    }
    const fileString = fileBuffer.toString();
    const fileSizeMB = fileBuffer.length / (1024 * 1024);
    console.log(`File size: ${fileSizeMB.toFixed(2)} MB`);

    // Pre-run one parser to show the target row and confirm it's readable
    try {
        const sampleRows = csvParseSync(fileBuffer);
        if (sampleRows.length > TARGET_ROW_INDEX) {
            console.log('Sample of target row:', sampleRows[TARGET_ROW_INDEX]);
        } else {
            console.warn(`Warning: File has fewer than ${TARGET_ROW_INDEX + 1} rows. Tests will run but may not retrieve data.`);
        }
    } catch(e) {
        console.error('Could not pre-parse the file to get sample row.', e);
        process.exit(1);
    }
    console.log('\n');

    // Synchronous Benchmark Suite
    await new Promise((resolve, reject) => {
        const syncSuite = new Suite('Sync (Parse only) Benchmark');
        console.log('--- Running: Sync (Parse only) Benchmarks ---');

        syncSuite
          .add('cisv (sync)', () => {
            const parser = new cisvParser();
            parser.write(fileBuffer);
            parser.end();
          })
          .add('csv-parse (sync)', () => {
            csvParseSync(fileBuffer);
          })
          .add('papaparse (sync)', () => {
            Papa.parse(fileString, { fastMode: true });
          })
          .on('cycle', (event) => logCycle(event, 'sync'))
          .on('error', reject)
          .on('complete', function() {
            console.log(`\n>>>>> Fastest Sync is ${this.filter('fastest').map('name')}\n`);
            resolve();
          })
          .run();
    });

    console.log('-'.repeat(50) + '\n');
    await sleep(3000);

    // Synchronous Benchmark Suite with data access
    await new Promise((resolve, reject) => {
        const syncSuite = new Suite('Sync (Parse + Access) Benchmark');
        console.log('--- Running: Sync (Parse + Access) Benchmarks ---');

        syncSuite
          .add('cisv (sync)', () => {
            const parser = new cisvParser();
            parser.write(fileBuffer);
            parser.end();
            const rows = parser.getRows();
            const specificRow = rows[TARGET_ROW_INDEX];
          })
          .add('csv-parse (sync)', () => {
            const rows = csvParseSync(fileBuffer);
            const specificRow = rows[TARGET_ROW_INDEX];
          })
          .add('papaparse (sync)', () => {
            const result = Papa.parse(fileString, { fastMode: true });
            const specificRow = result.data[TARGET_ROW_INDEX];
          })
          .on('cycle', (event) => logCycle(event, 'sync_data'))
          .on('error', reject)
          .on('complete', function() {
            console.log(`\n>>>>> Fastest Sync is ${this.filter('fastest').map('name')}\n`);
            resolve();
          })
          .run();
    });

    console.log('-'.repeat(50) + '\n');
    await sleep(3000);

    // Asynchronous Benchmark Suite
    await new Promise((resolve, reject) => {
        const asyncSuite = new Suite('Async (Parse only) Benchmark');
        console.log('--- Running: Async (Parse only) Benchmarks ---');

        asyncSuite
          .add('cisv (async/stream)', {
            defer: true,
            fn: (deferred) => {
              const parser = new cisvParser();
              const readable = stream.Readable.from(fileBuffer);
              readable
                .on('data', (chunk) => parser.write(chunk))
                .on('end', () => {
                  parser.end();
                  deferred.resolve();
                })
                .on('error', (err) => deferred.reject(err));
            }
          })
          .add('papaparse (async/stream)', {
            defer: true,
            fn: (deferred) => {
              const records = [];
              const readable = stream.Readable.from(fileBuffer);
              Papa.parse(readable, {
                step: (result) => records.push(result.data),
                complete: () => {
                    deferred.resolve();
                },
                error: (err) => deferred.reject(err)
              });
            }
          })
          .add('neat-csv (async/promise)', {
            defer: true,
            fn: (deferred) => {
              neatCsv(fileBuffer)
                .then(() => {
                    deferred.resolve()
                })
                .catch((err) => deferred.reject(err));
            }
          })
          .on('cycle', (event) => logCycle(event, 'async'))
          .on('error', reject)
          .on('complete', function() {
            console.log(`\n>>>>> Fastest Async is ${this.filter('fastest').map('name')}\n`);
            resolve();
          })
          .run({ async: true });
    });

    console.log('-'.repeat(50) + '\n');
    await sleep(3000);

    // Asynchronous Benchmark Suite with data access
    await new Promise((resolve, reject) => {
        const asyncSuite = new Suite('Async (Parse + Access) Benchmark');
        console.log('--- Running: Async (Parse + Access) Benchmarks ---');

        asyncSuite
          .add('cisv (async/stream)', {
            defer: true,
            fn: (deferred) => {
              const parser = new cisvParser();
              const readable = stream.Readable.from(fileBuffer);
              readable
                .on('data', (chunk) => parser.write(chunk))
                .on('end', () => {
                  parser.end();
                  const rows = parser.getRows();
                  const specificRow = rows[TARGET_ROW_INDEX];
                  deferred.resolve();
                })
                .on('error', (err) => deferred.reject(err));
            }
          })
          .add('papaparse (async/stream)', {
            defer: true,
            fn: (deferred) => {
              const records = [];
              const readable = stream.Readable.from(fileBuffer);
              Papa.parse(readable, {
                step: (result) => records.push(result.data),
                complete: () => {
                    const specificRow = records[TARGET_ROW_INDEX];
                    deferred.resolve();
                },
                error: (err) => deferred.reject(err)
              });
            }
          })
          .add('neat-csv (async/promise)', {
            defer: true,
            fn: (deferred) => {
              neatCsv(fileBuffer)
                .then((rows) => {
                    const specificRow = rows[TARGET_ROW_INDEX];
                    deferred.resolve()
                })
                .catch((err) => deferred.reject(err));
            }
          })
          .on('cycle', (event) => logCycle(event, 'async_data'))
          .on('error', reject)
          .on('complete', function() {
            console.log(`\n>>>>> Fastest Async is ${this.filter('fastest').map('name')}\n`);
            resolve();
          })
          .run({ async: true });
    });

    generateMarkdownReport();

    function logCycle(event, type) {
        const stats = event.target.stats;
        const meanTime = stats.mean;
        const opsPerSec = event.target.hz;
        const speedMBps = (fileSizeMB * opsPerSec);

        const resultLine = `  ${String(event.target)}\n    Speed: ${speedMBps.toFixed(2)} MB/s | Avg Time: ${(meanTime * 1000).toFixed(2)} ms | Ops/sec: ${opsPerSec.toFixed(0)}`;
        console.log(resultLine);

        results[type].push({
            name: String(event.target.name),
            speed: speedMBps.toFixed(2),
            avgTime: (meanTime * 1000).toFixed(2),
            ops: opsPerSec.toFixed(0)
        });

        console.log('    (cooling down...)\n');
    }

    function generateMarkdownReport() {
        console.log('-'.repeat(50));
        console.log('\nBenchmark Results Table (Markdown)\n');

        console.log('### Synchronous Results\n');
        let syncTable = '| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |\n';
        syncTable +=    '|--------------------|--------------|---------------|----------------|\n';
        results.sync.forEach(r => {
            syncTable += `| ${r.name.padEnd(18)} | ${r.speed.padEnd(12)} | ${r.avgTime.padEnd(13)} | ${r.ops.padEnd(14)} |\n`;
        });
        console.log(syncTable);

        console.log('### Synchronous Results (with data access)\n');
        let syncTabled = '| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |\n';
        syncTabled +=    '|--------------------|--------------|---------------|----------------|\n';
        results.sync_data.forEach(r => {
            syncTabled += `| ${r.name.padEnd(18)} | ${r.speed.padEnd(12)} | ${r.avgTime.padEnd(13)} | ${r.ops.padEnd(14)} |\n`;
        });
        console.log(syncTabled);

        console.log('\n### Asynchronous Results\n');
        let asyncTable = '| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |\n';
        asyncTable +=    '|--------------------------|--------------|---------------|----------------|\n';
        results.async.forEach(r => {
            asyncTable += `| ${r.name.padEnd(24)} | ${r.speed.padEnd(12)} | ${r.avgTime.padEnd(13)} | ${r.ops.padEnd(14)} |\n`;
        });
        console.log(asyncTable);

        console.log('\n### Asynchronous Results (with data access)\n');
        let asyncTabled = '| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |\n';
        asyncTabled +=    '|--------------------------|--------------|---------------|----------------|\n';
        results.async_data.forEach(r => {
            asyncTabled += `| ${r.name.padEnd(24)} | ${r.speed.padEnd(12)} | ${r.avgTime.padEnd(13)} | ${r.ops.padEnd(14)} |\n`;
        });
        console.log(asyncTabled);
    }
}

runAllBenchmarks().catch(err => {
    console.error('A benchmark test failed and was caught:', err);
});
