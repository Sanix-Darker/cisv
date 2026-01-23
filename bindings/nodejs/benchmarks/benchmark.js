'use strict';
/**
 * CISV Node.js Benchmark
 *
 * Compares cisv Node.js binding against popular CSV parsing libraries.
 *
 * Libraries compared:
 * - cisv: High-performance C parser with SIMD optimizations
 * - udsv: Ultra-fast DSV parser
 * - papaparse: Popular browser/node CSV parser
 * - csv-parse: Node.js stream/sync CSV parser
 * - d3-dsv: D3's delimiter-separated values parser
 * - fast-csv: Fast CSV parser with streaming support
 * - neat-csv: Promise-based CSV parser
 *
 * Usage:
 *   node benchmark.js [options]
 *
 * Options:
 *   --rows=N        Number of rows to generate (default: 100000)
 *   --cols=N        Number of columns (default: 5)
 *   --file=PATH     Use existing CSV file instead of generating
 *   --iterations=N  Number of iterations (default: 5)
 *   --fast          Skip slow libraries (neat-csv, fast-csv)
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const { performance } = require('perf_hooks');

// Parse command line arguments
function parseArgs() {
    const args = {
        rows: 100000,
        cols: 5,
        file: null,
        iterations: 5,
        fast: false,
    };

    for (let i = 2; i < process.argv.length; i++) {
        const arg = process.argv[i];
        let match;

        if ((match = arg.match(/^--rows=(\d+)$/))) {
            args.rows = parseInt(match[1], 10);
        } else if ((match = arg.match(/^--cols=(\d+)$/))) {
            args.cols = parseInt(match[1], 10);
        } else if ((match = arg.match(/^--file=(.+)$/))) {
            args.file = match[1];
        } else if ((match = arg.match(/^--iterations=(\d+)$/))) {
            args.iterations = parseInt(match[1], 10);
        } else if (arg === '--fast') {
            args.fast = true;
        }
    }

    return args;
}

/**
 * Generate a test CSV file
 */
function generateCsv(filepath, rows, cols) {
    console.log(`Generating CSV: ${rows.toLocaleString()} rows × ${cols} columns...`);
    const start = performance.now();

    const lines = [];

    // Header
    const header = [];
    for (let i = 0; i < cols; i++) {
        header.push(`col${i}`);
    }
    lines.push(header.join(','));

    // Data rows
    for (let row = 0; row < rows; row++) {
        const rowData = [];
        for (let col = 0; col < cols; col++) {
            rowData.push(`value_${row}_${col}`);
        }
        lines.push(rowData.join(','));

        if (row > 0 && row % 500000 === 0) {
            console.log(`  Generated ${row.toLocaleString()} rows...`);
        }
    }

    fs.writeFileSync(filepath, lines.join('\n'));

    const elapsed = (performance.now() - start) / 1000;
    const size = fs.statSync(filepath).size;
    const sizeMb = size / (1024 * 1024);
    console.log(`  Done in ${elapsed.toFixed(2)}s, file size: ${sizeMb.toFixed(1)} MB`);

    return size;
}

/**
 * Run a benchmark function multiple times
 */
async function benchmark(name, fn, iterations) {
    console.log(`Benchmarking ${name}...`);

    const times = [];
    let rowCount = 0;

    for (let i = 0; i < iterations; i++) {
        const start = performance.now();
        try {
            const result = await fn();
            rowCount = Array.isArray(result) ? result.length : result;
        } catch (e) {
            console.log(`  Error: ${e.message}`);
            return null;
        }
        const elapsed = (performance.now() - start) / 1000;
        times.push(elapsed);
    }

    const avgTime = times.reduce((a, b) => a + b, 0) / times.length;

    return {
        time: avgTime,
        rows: rowCount,
        iterations: times.length,
    };
}

/**
 * Format throughput as MB/s
 */
function formatThroughput(fileSize, time) {
    if (time > 0) {
        const mbPerSec = (fileSize / (1024 * 1024)) / time;
        return `${mbPerSec.toFixed(1)} MB/s`;
    }
    return 'N/A';
}

/**
 * Load libraries dynamically to handle missing dependencies
 */
function tryRequire(name) {
    try {
        return require(name);
    } catch (e) {
        return null;
    }
}

async function main() {
    const args = parseArgs();

    // Load libraries
    const cisv = tryRequire('../cisv');
    const csvParseSync = tryRequire('csv-parse/sync');
    const Papa = tryRequire('papaparse');
    const fastCsv = tryRequire('fast-csv');
    const udsv = tryRequire('udsv');
    const d3 = tryRequire('d3-dsv');
    let neatCsv = null;
    try {
        neatCsv = (await import('neat-csv')).default;
    } catch (e) {
        // neat-csv not available
    }

    console.log('============================================================');
    console.log('CISV Node.js Benchmark');
    console.log('============================================================\n');

    // Generate or use existing file
    let filepath;
    let fileSize;

    if (args.file) {
        filepath = path.resolve(args.file);
        if (!fs.existsSync(filepath)) {
            console.error(`Error: File not found: ${filepath}`);
            process.exit(1);
        }
        fileSize = fs.statSync(filepath).size;
        console.log(`Using existing file: ${filepath} (${(fileSize / (1024 * 1024)).toFixed(1)} MB)`);
    } else {
        filepath = path.join(os.tmpdir(), `cisv_benchmark_${process.pid}.csv`);
        fileSize = generateCsv(filepath, args.rows, args.cols);
    }

    console.log(`\n${'='.repeat(60)}`);
    console.log(`BENCHMARK: ${args.rows.toLocaleString()} rows × ${args.cols} columns`);
    console.log(`File size: ${(fileSize / (1024 * 1024)).toFixed(1)} MB`);
    console.log(`Iterations: ${args.iterations}`);
    console.log(`${'='.repeat(60)}\n`);

    // Read file contents (some parsers need string, some need buffer)
    const fileBuffer = fs.readFileSync(filepath);
    const fileString = fileBuffer.toString();

    const results = {};

    // Benchmark: cisv
    if (cisv && cisv.cisvParser) {
        results['cisv'] = await benchmark('cisv', () => {
            const parser = new cisv.cisvParser();
            parser.write(fileBuffer);
            parser.end();
            return parser.getRows();
        }, args.iterations);
    } else {
        console.log('Benchmarking cisv...');
        console.log('  Skipped: cisv not available');
    }

    // Benchmark: udsv
    if (udsv) {
        results['udsv'] = await benchmark('udsv', () => {
            const schema = udsv.inferSchema(fileString);
            const parser = udsv.initParser(schema);
            return parser.stringArrs(fileString);
        }, args.iterations);
    } else {
        console.log('Benchmarking udsv...');
        console.log('  Skipped: udsv not installed');
    }

    // Benchmark: papaparse
    if (Papa) {
        results['papaparse'] = await benchmark('papaparse', () => {
            const result = Papa.parse(fileString, { fastMode: true });
            return result.data;
        }, args.iterations);
    } else {
        console.log('Benchmarking papaparse...');
        console.log('  Skipped: papaparse not installed');
    }

    // Benchmark: csv-parse
    if (csvParseSync) {
        results['csv-parse'] = await benchmark('csv-parse', () => {
            return csvParseSync.parse(fileBuffer);
        }, args.iterations);
    } else {
        console.log('Benchmarking csv-parse...');
        console.log('  Skipped: csv-parse not installed');
    }

    // Benchmark: d3-dsv
    if (d3) {
        results['d3-dsv'] = await benchmark('d3-dsv', () => {
            return d3.csvParseRows(fileString);
        }, args.iterations);
    } else {
        console.log('Benchmarking d3-dsv...');
        console.log('  Skipped: d3-dsv not installed');
    }

    // Slow libraries (skip with --fast flag)
    if (!args.fast) {
        // Benchmark: fast-csv
        if (fastCsv) {
            results['fast-csv'] = await benchmark('fast-csv', () => {
                return new Promise((resolve, reject) => {
                    const rows = [];
                    const stream = require('stream');
                    const readable = stream.Readable.from(fileBuffer);
                    readable
                        .pipe(fastCsv.parse())
                        .on('data', (row) => rows.push(row))
                        .on('end', () => resolve(rows))
                        .on('error', reject);
                });
            }, args.iterations);
        } else {
            console.log('Benchmarking fast-csv...');
            console.log('  Skipped: fast-csv not installed');
        }

        // Benchmark: neat-csv
        if (neatCsv) {
            results['neat-csv'] = await benchmark('neat-csv', async () => {
                return await neatCsv(fileBuffer);
            }, args.iterations);
        } else {
            console.log('Benchmarking neat-csv...');
            console.log('  Skipped: neat-csv not installed');
        }
    }

    // Print results
    console.log(`\n${'='.repeat(60)}`);
    console.log('RESULTS');
    console.log(`${'='.repeat(60)}`);
    console.log(`${'Library'.padEnd(16)} ${'Parse Time'.padStart(12)} ${'Throughput'.padStart(14)} ${'Rows'.padStart(12)}`);
    console.log('-'.repeat(60));

    // Sort by time (fastest first)
    const sortedResults = Object.entries(results)
        .filter(([_, result]) => result !== null)
        .sort(([_, a], [__, b]) => a.time - b.time);

    for (const [name, result] of sortedResults) {
        const throughput = formatThroughput(fileSize, result.time);
        console.log(
            `${name.padEnd(16)} ${result.time.toFixed(3).padStart(10)}s ${throughput.padStart(14)} ${result.rows.toLocaleString().padStart(12)}`
        );
    }

    // Cleanup
    if (!args.file) {
        fs.unlinkSync(filepath);
        console.log('\nCleaned up temporary file');
    }

    console.log('\nBenchmark complete!');
}

main().catch((err) => {
    console.error('Benchmark failed:', err);
    process.exit(1);
});
