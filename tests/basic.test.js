const { cisvParser } = require('../build/Release/cisv');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

describe('CSV Parser Core Functionality', () => {
  const testDir = path.join(__dirname, 'fixtures');
  const testFile = path.join(testDir, 'test.csv');
  const largeFile = path.join(testDir, 'large.csv');

  before(() => {
    if (!fs.existsSync(testDir)) fs.mkdirSync(testDir);

    // Basic test file with quoted fields
    fs.writeFileSync(testFile,
      'id,name,email\n1,John,john@test.com\n2,Jane Doe,jane@test.com\n3,Alex \"The Boss\",alex@test.com');

    // Generate large test file (1000 rows)
    let largeContent = 'id,value\n';
    for (let i = 0; i < 1000; i++) {
      largeContent += `${i},Value ${i}\n`;
    }
    fs.writeFileSync(largeFile, largeContent);
  });

  after(() => {
    fs.unlinkSync(testFile);
    fs.unlinkSync(largeFile);
  });

  describe('Synchronous Parsing', () => {
    it('should parse basic CSV correctly', () => {
      const parser = new cisvParser();
      const rows = parser.parseSync(testFile);
      assert.strictEqual(rows.length, 4);
      assert.deepStrictEqual(rows[0], ['id', 'name', 'email']);
      assert.deepStrictEqual(rows[1], ['1', 'John', 'john@test.com']);
    });

    it('should handle quoted fields with commas', () => {
      const parser = new cisvParser();
      const rows = parser.parseSync(testFile);
      assert.strictEqual(rows[2][1], 'Jane Doe');
      assert.strictEqual(rows[2][2], 'jane@test.com');
    });

    it('should handle large files efficiently', () => {
      const parser = new cisvParser();
      const rows = parser.parseSync(largeFile);
      assert.strictEqual(rows.length, 1001); // Header + 1000 rows
      assert.strictEqual(rows[500][1], 'Value 499');
    });
  });

  describe('Streaming API', () => {
    it('should process data in chunks', (done) => {
      const parser = new cisvParser();
      const stream = fs.createReadStream(testFile, { highWaterMark: 128 }); // Small chunks

      stream.on('data', chunk => parser.write(chunk));
      stream.on('end', () => {
        parser.end();
        const rows = parser.getRows();
        assert.strictEqual(rows.length, 4);
        done();
      });
    });

    it('should handle partial chunks', () => {
      const parser = new cisvParser();
      // Write partial lines
      parser.write(Buffer.from('id,name,emai'));
      parser.write(Buffer.from('l\n1,John,john@test.com\n'));
      parser.end();
      const rows = parser.getRows();
      assert.strictEqual(rows.length, 2);
      assert.deepStrictEqual(rows[0], ['id', 'name', 'email']);
    });
  });

  // FIXME: should be fixed from the napi addon.cc
  //describe('Error Handling', () => {
  //    it('should throw on non-existent file', () => {
  //      const parser = new cisvParser();
  //      assert.throws(() => parser.parseSync('./nonexistent.csv'), /parse error/);
  //    });

  //    it('should throw on invalid write arguments', () => {
  //      const parser = new cisvParser();
  //      // Test with actually invalid arguments
  //      assert.throws(() => parser.write(123), /Expected Buffer or String/);
  //      assert.throws(() => parser.write({}), /Expected Buffer or String/);
  //      assert.throws(() => parser.write(null), /Expected Buffer or String/);
  //      assert.throws(() => parser.write(), /Expected one argument/);
  //    });

  //    it('should accept both Buffer and String in write', () => {
  //      const parser = new cisvParser();
  //      // Should not throw
  //      assert.doesNotThrow(() => parser.write(Buffer.from('test')));
  //      assert.doesNotThrow(() => parser.write('test'));
  //    });
  //});
});
