import { join } from 'path';
import { createReadStream } from 'fs';

// Type declarations for your native module
declare module '../build/Release/cisv' {
  export class cisvParser {
    parseSync(path: string): string[][];
    write(chunk: Buffer): void;
    end(): void;
    getRows(): string[][];
  }
}

const { cisvParser } = require('../build/Release/cisv') as {
  cisvParser: typeof cisvParser
};

async function processCSV(filePath: string) {
  const parser = new cisvParser();
  const stream = createReadStream(filePath);

  return new Promise<string[][]>((resolve, reject) => {
    stream.on('data', chunk => parser.write(chunk));
    stream.on('end', () => {
      parser.end();
      resolve(parser.getRows());
    });
    stream.on('error', reject);
  });
}

(async () => {
  try {
    const rows = await processCSV(join(__dirname, '../fixtures/data.csv'));
    console.log(`Processed ${rows.length} rows`);
  } catch (err) {
    console.error('Error:', err);
  }
})();
