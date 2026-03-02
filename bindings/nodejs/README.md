# CISV Node.js Binding

Native Node-API binding for the CISV C core.

## Install

```bash
npm install cisv
```

From source in this repository:

```bash
cd bindings/nodejs
npm ci
npm run build
npm test
```

## Quick Start

```js
const { cisvParser } = require('cisv');

const parser = new cisvParser({ delimiter: ',', trim: true });
const rows = parser.parseSync('data.csv');

console.log(rows.length);
console.log(rows[0]);
```

## Parser API

### Constructor options

- `delimiter?: string` (first character used)
- `quote?: string` (first character used)
- `escape?: string | null` (`null` means RFC4180 doubled quote escaping)
- `comment?: string | null`
- `trim?: boolean`
- `skipEmptyLines?: boolean`
- `relaxed?: boolean`
- `skipLinesWithError?: boolean`
- `maxRowSize?: number`
- `fromLine?: number`
- `toLine?: number`

### Instance methods

- `parseSync(path: string): string[][]`
- `parse(path: string): Promise<string[][]>`
- `parseString(csv: string): string[][]`
- `write(chunk: Buffer | string): void`
- `end(): void`
- `getRows(): string[][]`
- `clear(): void`
- `setConfig(config): this`
- `getConfig(): object`
- `transform(fieldIndex: number, kindOrFn: string | Function, context?): this`
- `transformByName(fieldName: string, kindOrFn: string | Function, context?): this`
- `setHeaderFields(fields: string[]): void`
- `removeTransform(fieldIndex: number): this`
- `removeTransformByName(fieldName: string): this`
- `clearTransforms(): this`
- `getTransformInfo(): { cTransformCount: number, jsTransformCount: number, fieldIndices: number[] }`
- `getStats(): { rowCount: number, fieldCount: number, totalBytes: number, parseTime: number, currentLine: number }`
- `openIterator(path: string): this`
- `fetchRow(): string[] | null`
- `closeIterator(): this`
- `destroy(): void`

### Static methods

- `cisvParser.countRows(path: string): number`
- `cisvParser.countRowsWithConfig(path: string, config?): number`

## Transform Types

Built-in transform names:

- `uppercase`
- `lowercase`
- `trim`
- `to_int` (or `int`)
- `to_float` (or `float`)
- `hash_sha256` (or `sha256`)
- `base64_encode` (or `base64`)

## Examples

### Async parse

```js
const { cisvParser } = require('cisv');

(async () => {
  const parser = new cisvParser();
  const rows = await parser.parse('data.csv');
  console.log(rows.length);
})();
```

### Streaming chunks

```js
const fs = require('fs');
const { cisvParser } = require('cisv');

const parser = new cisvParser();
for (const chunk of [
  Buffer.from('id,name\n1,'),
  Buffer.from('john\n2,jane\n')
]) {
  parser.write(chunk);
}
parser.end();

console.log(parser.getRows());
```

### Iterator mode (low memory)

```js
const { cisvParser } = require('cisv');

const parser = new cisvParser({ delimiter: ',' });
parser.openIterator('large.csv');

let row;
while ((row = parser.fetchRow()) !== null) {
  if (row[0] === 'stop') break;
}

parser.closeIterator();
```

### Name-based transforms

```js
const { cisvParser } = require('cisv');

const parser = new cisvParser();
parser.setHeaderFields(['id', 'name', 'email']);
parser.transformByName('name', 'uppercase');

const rows = parser.parseString('id,name,email\n1,john,john@test.com');
console.log(rows[1][1]); // JOHN
```

## Notes

- Returned rows include the header row when the input has one.
- `removeTransform*` currently removes JavaScript transforms; C-transform removal by index/name is not fully implemented yet.
- `parse()` runs in a worker thread for non-transform workloads; when transforms are attached it preserves current synchronous transform behavior for compatibility.
