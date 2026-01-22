#!/usr/bin/env php
<?php
/**
 * CISV PHP Benchmark
 *
 * Compares cisv PHP extension against native PHP CSV parsing methods.
 *
 * CISV Benchmark Modes:
 * - cisv: Single-threaded parsing, returns array of rows
 * - cisv-parallel: Multi-threaded parsing (auto-detect CPU cores)
 * - cisv-bench: Multi-threaded, count only (raw parsing speed, no data marshaling)
 * - cisv-count: Fast row counting using SIMD
 *
 * Usage:
 *   php benchmark.php [--rows=N] [--file=/path/to/file.csv] [--iterations=N]
 *
 * Examples:
 *   php benchmark.php --rows=1000000
 *   php benchmark.php --file=/data/large.csv
 *   php benchmark.php --rows=100000 --iterations=10
 */

// Configuration
$config = [
    'rows' => 1000000,
    'cols' => 7,
    'iterations' => 5,
    'file' => null,
];

// Parse command line arguments
foreach ($argv as $arg) {
    if (preg_match('/^--rows=(\d+)$/', $arg, $m)) {
        $config['rows'] = (int)$m[1];
    } elseif (preg_match('/^--file=(.+)$/', $arg, $m)) {
        $config['file'] = $m[1];
    } elseif (preg_match('/^--iterations=(\d+)$/', $arg, $m)) {
        $config['iterations'] = (int)$m[1];
    } elseif (preg_match('/^--cols=(\d+)$/', $arg, $m)) {
        $config['cols'] = (int)$m[1];
    }
}

// Results storage
$results = [];

/**
 * Generate a test CSV file
 */
function generateCsv(string $filename, int $rows, int $cols): int {
    echo "Generating CSV: " . number_format($rows) . " rows × $cols columns...\n";
    $start = microtime(true);

    $fp = fopen($filename, 'w');

    // Header
    $headers = [];
    for ($i = 0; $i < $cols; $i++) {
        $headers[] = "col$i";
    }
    fputcsv($fp, $headers);

    // Data rows
    for ($row = 0; $row < $rows; $row++) {
        $data = [];
        for ($col = 0; $col < $cols; $col++) {
            $data[] = "value_{$row}_{$col}";
        }
        fputcsv($fp, $data);

        if ($row > 0 && $row % 500000 === 0) {
            echo "  Generated " . number_format($row) . " rows...\n";
        }
    }

    fclose($fp);

    $elapsed = microtime(true) - $start;
    $size = filesize($filename);
    $sizeMb = $size / (1024 * 1024);
    echo sprintf("  Done in %.2fs, file size: %.1f MB\n", $elapsed, $sizeMb);

    return $size;
}

/**
 * Run a benchmark function multiple times
 */
function benchmark(string $name, callable $fn, int $iterations): ?array {
    echo "Benchmarking $name...\n";

    $times = [];
    $rowCount = 0;

    for ($i = 0; $i < $iterations; $i++) {
        $start = microtime(true);
        try {
            $result = $fn();
            $rowCount = is_array($result) ? count($result) : $result;
        } catch (Exception $e) {
            echo "  Error: " . $e->getMessage() . "\n";
            return null;
        }
        $elapsed = microtime(true) - $start;
        $times[] = $elapsed;
    }

    $avgTime = array_sum($times) / count($times);

    return [
        'time' => $avgTime,
        'rows' => $rowCount,
        'iterations' => count($times),
    ];
}

/**
 * Format throughput as MB/s
 */
function formatThroughput(int $fileSize, float $time): string {
    if ($time > 0) {
        $mbPerSec = ($fileSize / (1024 * 1024)) / $time;
        return sprintf("%.1f MB/s", $mbPerSec);
    }
    return "N/A";
}

// Check for cisv extension
$cisvAvailable = extension_loaded('cisv') || class_exists('CisvParser');

echo "============================================================\n";
echo "CISV PHP Benchmark\n";
echo "============================================================\n\n";

// Generate or use existing file
if ($config['file']) {
    $filepath = $config['file'];
    if (!file_exists($filepath)) {
        echo "Error: File not found: $filepath\n";
        exit(1);
    }
    $fileSize = filesize($filepath);
    echo sprintf("Using existing file: %s (%.1f MB)\n", $filepath, $fileSize / (1024 * 1024));
} else {
    $filepath = sys_get_temp_dir() . '/cisv_benchmark_' . getmypid() . '.csv';
    $fileSize = generateCsv($filepath, $config['rows'], $config['cols']);
}

echo "\n============================================================\n";
echo sprintf("BENCHMARK: %s rows × %d columns\n", number_format($config['rows']), $config['cols']);
echo sprintf("File size: %.1f MB\n", $fileSize / (1024 * 1024));
echo sprintf("Iterations: %d\n", $config['iterations']);
echo "============================================================\n\n";

// Benchmark: cisv extension (parse - single-threaded)
if ($cisvAvailable) {
    $results['cisv'] = benchmark('cisv', function() use ($filepath) {
        $parser = new CisvParser();
        return $parser->parseFile($filepath);
    }, $config['iterations']);

    // Benchmark: cisv extension (parallel - multi-threaded)
    $results['cisv-parallel'] = benchmark('cisv-parallel', function() use ($filepath) {
        $parser = new CisvParser();
        return $parser->parseFileParallel($filepath, 0);  // 0 = auto-detect threads
    }, $config['iterations']);

    // Benchmark: cisv extension (benchmark mode - parallel, no data marshaling)
    $results['cisv-bench'] = benchmark('cisv-bench (parallel, count only)', function() use ($filepath) {
        $parser = new CisvParser();
        $result = $parser->parseFileBenchmark($filepath, 0);
        return $result['rows'];
    }, $config['iterations']);

    // Benchmark: cisv extension (count - fast row counting)
    $results['cisv-count'] = benchmark('cisv-count', function() use ($filepath) {
        return CisvParser::countRows($filepath);
    }, $config['iterations']);
} else {
    echo "cisv extension not loaded, skipping cisv benchmarks\n";
}

// Benchmark: fgetcsv
$results['fgetcsv'] = benchmark('fgetcsv', function() use ($filepath) {
    $rows = [];
    $fp = fopen($filepath, 'r');
    while (($row = fgetcsv($fp)) !== false) {
        $rows[] = $row;
    }
    fclose($fp);
    return $rows;
}, $config['iterations']);

// Benchmark: str_getcsv
$results['str_getcsv'] = benchmark('str_getcsv', function() use ($filepath) {
    $content = file_get_contents($filepath);
    $lines = explode("\n", $content);
    $rows = [];
    foreach ($lines as $line) {
        if ($line) {
            $rows[] = str_getcsv($line);
        }
    }
    return $rows;
}, $config['iterations']);

// Benchmark: SplFileObject
$results['SplFileObject'] = benchmark('SplFileObject', function() use ($filepath) {
    $rows = [];
    $spl = new SplFileObject($filepath);
    $spl->setFlags(SplFileObject::READ_CSV);
    foreach ($spl as $row) {
        if ($row[0] !== null) {
            $rows[] = $row;
        }
    }
    return $rows;
}, $config['iterations']);

// Benchmark: explode (simple but fast for well-formed CSV)
$results['explode'] = benchmark('explode', function() use ($filepath) {
    $content = file_get_contents($filepath);
    $lines = explode("\n", $content);
    $rows = [];
    foreach ($lines as $line) {
        if ($line) {
            $rows[] = explode(',', $line);
        }
    }
    return $rows;
}, $config['iterations']);

// Benchmark: array_map + str_getcsv
$results['array_map'] = benchmark('array_map + str_getcsv', function() use ($filepath) {
    $content = file($filepath, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    return array_map('str_getcsv', $content);
}, $config['iterations']);

// Benchmark: preg_split
$results['preg_split'] = benchmark('preg_split', function() use ($filepath) {
    $content = file_get_contents($filepath);
    $lines = preg_split('/\r?\n/', $content);
    $rows = [];
    foreach ($lines as $line) {
        if ($line) {
            $rows[] = preg_split('/,/', $line);
        }
    }
    return $rows;
}, $config['iterations']);

// Check for league/csv
if (file_exists('/tmp/csv_bench/vendor/autoload.php')) {
    require '/tmp/csv_bench/vendor/autoload.php';

    $results['league/csv'] = benchmark('league/csv', function() use ($filepath) {
        $csv = \League\Csv\Reader::createFromPath($filepath, 'r');
        $csv->setHeaderOffset(0);
        return iterator_to_array($csv->getRecords());
    }, $config['iterations']);
}

// Print results
echo "\n============================================================\n";
echo "RESULTS\n";
echo "============================================================\n";
echo sprintf("%-20s %12s %14s %12s\n", "Library", "Parse Time", "Throughput", "Rows");
echo str_repeat("-", 60) . "\n";

// Sort by time
uasort($results, function($a, $b) {
    if ($a === null) return 1;
    if ($b === null) return -1;
    return $a['time'] <=> $b['time'];
});

foreach ($results as $name => $result) {
    if ($result) {
        $throughput = formatThroughput($fileSize, $result['time']);
        echo sprintf("%-20s %10.3fs %14s %12s\n",
            $name,
            $result['time'],
            $throughput,
            number_format($result['rows'])
        );
    }
}

// Cleanup
if (!$config['file']) {
    unlink($filepath);
    echo "\nCleaned up temporary file\n";
}

echo "\nBenchmark complete!\n";
