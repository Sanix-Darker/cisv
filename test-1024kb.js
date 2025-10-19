const fs = require('fs');
const { cisvParser } = require('./build/Release/cisv');

const BENCH_FILE = '/home/dk/Downloads/customers-6091.csv';

const csvStr = fs.readFileSync(BENCH_FILE, 'utf8');
const parser = new cisvParser();
const rows = parser.parseString(csvStr);

console.log(rows);
