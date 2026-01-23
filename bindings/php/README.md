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

    /**
     * Open a file for row-by-row iteration.
     *
     * @param string $filename Path to CSV file
     * @return $this Fluent interface
     */
    public function openIterator(string $filename): self;

    /**
     * Fetch the next row from the iterator.
     *
     * @return array|false Array of field values, or false at EOF
     */
    public function fetchRow(): array|false;

    /**
     * Close the iterator and release resources.
     *
     * @return $this Fluent interface
     */
    public function closeIterator(): self;
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

### Row-by-Row Iterator (Memory Efficient)

For large files, use the iterator API to process rows one at a time without loading the entire file into memory:

```php
$parser = new CisvParser(['delimiter' => ',', 'trim' => true]);
$parser->openIterator('large.csv');

while (($row = $parser->fetchRow()) !== false) {
    print_r($row);

    // Early exit is fully supported - stops parsing immediately
    if ($row[0] === 'stop') {
        break;
    }
}

$parser->closeIterator();
```

### Iterator with Progress Tracking

```php
// Count before processing
$total = CisvParser::countRows('huge.csv');
echo "Processing $total rows...\n";

$parser = new CisvParser(['skip_empty' => true]);
$parser->openIterator('huge.csv');

$processed = 0;
while (($row = $parser->fetchRow()) !== false) {
    // Your processing logic
    $processed++;

    if ($processed % 100000 === 0) {
        echo "Processed $processed / $total\n";
    }
}

$parser->closeIterator();
echo "Done! Processed $processed rows.\n";
```

### Process Large Files (Load All)

```php
// Count before processing
$total = CisvParser::countRows('huge.csv');
echo "Processing $total rows...\n";

// Parse file - loads all rows into memory
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
// CISV - Load all (fast)
$parser = new CisvParser();
$rows = $parser->parseFile('large.csv');

// CISV - Iterator (fast, memory efficient)
$parser = new CisvParser();
$parser->openIterator('large.csv');
while (($row = $parser->fetchRow()) !== false) {
    // Process row
}
$parser->closeIterator();

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
