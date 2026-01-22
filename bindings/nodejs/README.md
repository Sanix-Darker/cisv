# CISV

![License](https://img.shields.io/badge/license-MIT-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Size](https://deno.bundlejs.com/badge?q=spring-easing)
![Downloads](https://badgen.net/npm/dw/cisv)

## INSTALLATION

### NODE.JS PACKAGE
```bash
npm install cisv
```

### BUILD FROM SOURCE (NODE.JS ADDON)
```bash
npm install -g node-gyp
```

## QUICK START

### NODE.JS
```javascript
const { cisvParser } = require('cisv');

// Basic usage
const parser = new cisvParser();
const rows = parser.parseSync('./data.csv');

// With configuration (optional)
const tsv_parser = new cisvParser({
    delimiter: '\t',
    quote: "'",
    trim: true
});
const tsv_rows = tsv_parser.parseSync('./data.tsv');
```


## CONFIGURATION OPTIONS

### Parser Configuration

```javascript
const parser = new cisvParser({
    // Field delimiter character (default: ',')
    delimiter: ',',

    // Quote character (default: '"')
    quote: '"',

    // Escape character (null for RFC4180 "" style, default: null)
    escape: null,

    // Comment character to skip lines (default: null)
    comment: '#',

    // Trim whitespace from fields (default: false)
    trim: true,

    // Skip empty lines (default: false)
    skipEmptyLines: true,

    // Use relaxed parsing rules (default: false)
    relaxed: false,

    // Skip lines with parse errors (default: false)
    skipLinesWithError: true,

    // Maximum row size in bytes (0 = unlimited, default: 0)
    maxRowSize: 1048576,

    // Start parsing from line N (1-based, default: 1)
    fromLine: 10,

    // Stop parsing at line N (0 = until end, default: 0)
    toLine: 1000
});
```

### Dynamic Configuration

```javascript
// Set configuration after creation
parser.setConfig({
    delimiter: ';',
    quote: "'",
    trim: true
});

// Get current configuration
const config = parser.getConfig();
console.log(config);
```

## API REFERENCE

### TYPESCRIPT DEFINITIONS
```typescript
interface CisvConfig {
    delimiter?: string;
    quote?: string;
    escape?: string | null;
    comment?: string | null;
    trim?: boolean;
    skipEmptyLines?: boolean;
    relaxed?: boolean;
    skipLinesWithError?: boolean;
    maxRowSize?: number;
    fromLine?: number;
    toLine?: number;
}

interface ParsedRow extends Array<string> {}

interface ParseStats {
    rowCount: number;
    fieldCount: number;
    totalBytes: number;
    parseTime: number;
    currentLine: number;
}

interface TransformInfo {
    cTransformCount: number;
    jsTransformCount: number;
    fieldIndices: number[];
}

class cisvParser {
    constructor(config?: CisvConfig);
    parseSync(path: string): ParsedRow[];
    parse(path: string): Promise<ParsedRow[]>;
    parseString(csv: string): ParsedRow[];
    write(chunk: string | Buffer): void;
    end(): void;
    getRows(): ParsedRow[];
    clear(): void;
    setConfig(config: CisvConfig): void;
    getConfig(): CisvConfig;
    transform(fieldIndex: number, type: string | Function): this;
    removeTransform(fieldIndex: number): this;
    clearTransforms(): this;
    getStats(): ParseStats;
    getTransformInfo(): TransformInfo;
    destroy(): void;

    static countRows(path: string): number;
    static countRowsWithConfig(path: string, config?: CisvConfig): number;
}
```

### BASIC PARSING

```javascript
import { cisvParser } from "cisv";

// Default configuration (standard CSV)
const parser = new cisvParser();
const rows = parser.parseSync('data.csv');

// Custom configuration (TSV with single quotes)
const tsvParser = new cisvParser({
    delimiter: '\t',
    quote: "'"
});
const tsvRows = tsvParser.parseSync('data.tsv');

// Parse specific line range
const rangeParser = new cisvParser({
    fromLine: 100,
    toLine: 1000
});
const subset = rangeParser.parseSync('large.csv');

// Skip comments and empty lines
const cleanParser = new cisvParser({
    comment: '#',
    skipEmptyLines: true,
    trim: true
});
const cleanData = cleanParser.parseSync('config.csv');
```

### STREAMING

```javascript
import { cisvParser } from "cisv";
import fs from 'fs';

const streamParser = new cisvParser({
    delimiter: ',',
    trim: true
});

const stream = fs.createReadStream('huge-file.csv');

stream.on('data', chunk => streamParser.write(chunk));
stream.on('end', () => {
    streamParser.end();
    const results = streamParser.getRows();
    console.log(`Parsed ${results.length} rows`);
});
```

### DATA TRANSFORMATION

```javascript
const parser = new cisvParser();

// Built-in C transforms (optimized)
parser
    .transform(0, 'uppercase')      // Column 0 to uppercase
    .transform(1, 'lowercase')       // Column 1 to lowercase
    .transform(2, 'trim')           // Column 2 trim whitespace
    .transform(3, 'to_int')         // Column 3 to integer
    .transform(4, 'to_float')       // Column 4 to float
    .transform(5, 'base64_encode')  // Column 5 to base64
    .transform(6, 'hash_sha256');   // Column 6 to SHA256

// Custom fieldname transform :
parser
    .transform('name', 'uppercase');

// Custom row transform :
parser
    .transformRow((row, rowObj) => {console.log(row}});

// Custom JavaScript transforms
parser.transform(7, value => new Date(value).toISOString());

// Apply to all fields
parser.transform(-1, value => value.replace(/[^\w\s]/gi, ''));

const transformed = parser.parseSync('data.csv');
```

### ROW COUNTING

```javascript
import { cisvParser } from "cisv";

// Fast row counting without parsing
const count = cisvParser.countRows('large.csv');

// Count with specific configuration
const tsvCount = cisvParser.countRowsWithConfig('data.tsv', {
    delimiter: '\t',
    skipEmptyLines: true,
    fromLine: 10,
    toLine: 1000
});
```

### ROW-BY-ROW ITERATION

The iterator API provides fgetcsv-style streaming with minimal memory footprint and early exit support.

```javascript
import { cisvParser } from "cisv";

const parser = new cisvParser({ delimiter: ',', trim: true });

// Open iterator for a file
parser.openIterator('/path/to/large.csv');

// Fetch rows one at a time
let row;
while ((row = parser.fetchRow()) !== null) {
    console.log(row);  // string[]

    // Early exit - no wasted work
    if (row[0] === 'stop') {
        break;
    }
}

// Close iterator when done
parser.closeIterator();

// Methods support chaining
parser.openIterator('data.csv')
      .closeIterator();
```

**Iterator Methods:**

| Method | Description |
|--------|-------------|
| `openIterator(path)` | Open a file for row-by-row iteration |
| `fetchRow()` | Get next row as `string[]`, or `null` if at EOF |
| `closeIterator()` | Close iterator and release resources |

**Notes:**
- The iterator uses the parser's current configuration (delimiter, quote, trim, etc.)
- Calling `destroy()` automatically closes any open iterator
- Only one iterator can be open at a time per parser instance
- Breaking out of iteration and calling `closeIterator()` stops parsing immediately
