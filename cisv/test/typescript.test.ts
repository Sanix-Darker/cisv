import { strict as assert } from 'assert';
import * as fs from 'fs';
import * as path from 'path';
import { cisvParser } from '../index';

describe('TypeScript Integration', () => {
  const testFile = path.join(__dirname, 'fixtures', 'typescript.csv');

  before(() => {
    fs.writeFileSync(testFile,
      'id,name\n1,Test\n2,"Quoted,Name"');
  });

  it('should work with TypeScript types', () => {
    const parser = new cisvParser();
    const rows = parser.parseSync(testFile);

    assert.strictEqual(rows.length, 2);
    assert.deepStrictEqual(rows[0], ['1', 'Test']);
    assert.deepStrictEqual(rows[1], ['2', 'Quoted,Name']);
  });

  it('should type check streaming API', async () => {
    const parser = new cisvParser();
    const stream = fs.createReadStream(testFile);

    await new Promise<void>((resolve) => {
      stream.on('data', (chunk: Buffer | string) => {
          if (typeof chunk === 'string') {
            parser.write(Buffer.from(chunk));
          } else {
            parser.write(chunk);
          }
      });
      stream.on('end', () => {
        parser.end();
        const rows = parser.getRows();
        assert.strictEqual(rows.length, 2);
        resolve();
      });
    });
  });
});
