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
