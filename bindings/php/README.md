# CISV PHP Extension

High-performance CSV parser with SIMD optimizations for PHP.

## Requirements

- PHP 7.4+ or PHP 8.x
- PHP development headers (`php-dev`)
- CISV core library

## Installation

### Build Core Library First

```bash
cd ../../core
make
```

### Build PHP Extension

```bash
phpize
./configure --with-cisv
make
sudo make install
```

Then add to your `php.ini`:

```ini
extension=cisv.so
```

Or load dynamically:

```php
dl('cisv.so');
```

## Quick Start

```php
<?php

// Create parser with default options
$parser = new CisvParser();
$rows = $parser->parseFile('data.csv');

foreach ($rows as $row) {
    print_r($row);
}

// Create parser with custom options
$parser = new CisvParser([
    'delimiter' => ';',
    'quote' => "'",
    'trim' => true,
    'skip_empty' => true
]);

$rows = $parser->parseFile('data.csv');

// Parse from string
$csv = "name,age,email\nJohn,30,john@example.com";
$rows = $parser->parseString($csv);

// Fast row counting
$count = CisvParser::countRows('large.csv');
echo "Total rows: $count\n";
```

## API Reference

### CisvParser Class

```php
class CisvParser {
    /**
     * Create a new CSV parser.
     *
     * @param array $options Configuration options
     */
    public function __construct(array $options = []);

    /**
     * Parse a CSV file and return all rows.
     *
     * @param string $filename Path to CSV file
     * @return array Array of row arrays
     */
    public function parseFile(string $filename): array;

    /**
     * Parse a CSV string and return all rows.
     *
     * @param string $csv CSV content
     * @return array Array of row arrays
     */
    public function parseString(string $csv): array;

    /**
     * Count rows in a CSV file without full parsing.
     *
     * @param string $filename Path to CSV file
     * @return int Number of rows
     */
    public static function countRows(string $filename): int;

    /**
     * Set the field delimiter.
     *
     * @param string $delimiter Single character delimiter
     * @return $this Fluent interface
     */
    public function setDelimiter(string $delimiter): self;

    /**
     * Set the quote character.
     *
     * @param string $quote Single character quote
     * @return $this Fluent interface
     */
    public function setQuote(string $quote): self;
}
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `delimiter` | string | `','` | Field delimiter character |
| `quote` | string | `'"'` | Quote character |
| `trim` | bool | `false` | Trim whitespace from fields |
| `skip_empty` | bool | `false` | Skip empty lines |

## Examples

### TSV Parsing

```php
$parser = new CisvParser(['delimiter' => "\t"]);
$rows = $parser->parseFile('data.tsv');
```

### Fluent Configuration

```php
$parser = new CisvParser();
$rows = $parser
    ->setDelimiter(';')
    ->setQuote("'")
    ->parseFile('data.csv');
```

### Parse with Headers

```php
$parser = new CisvParser(['trim' => true]);
$rows = $parser->parseFile('data.csv');

// First row as headers
$headers = array_shift($rows);

// Convert to associative arrays
$data = array_map(function($row) use ($headers) {
    return array_combine($headers, $row);
}, $rows);

print_r($data);
```

### Process Large Files

```php
// Count before processing
$total = CisvParser::countRows('huge.csv');
echo "Processing $total rows...\n";

// Parse file
$parser = new CisvParser(['skip_empty' => true]);
$rows = $parser->parseFile('huge.csv');

// Process rows
$processed = 0;
foreach ($rows as $row) {
    // Your processing logic
    $processed++;

    if ($processed % 100000 === 0) {
        echo "Processed $processed / $total\n";
    }
}
```

## Performance

CISV uses SIMD optimizations (AVX-512, AVX2, SSE2) for high-performance parsing. Typical performance on modern hardware:

- 500MB+ CSV files parsed in under 1 second
- 10-50x faster than `fgetcsv()` and `str_getcsv()`
- Zero-copy memory-mapped file parsing

## Comparison with Native PHP

```php
// CISV (fast)
$parser = new CisvParser();
$rows = $parser->parseFile('large.csv');

// Native PHP (slow)
$rows = [];
$fp = fopen('large.csv', 'r');
while (($row = fgetcsv($fp)) !== false) {
    $rows[] = $row;
}
fclose($fp);
```

## License

MIT
