# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-02-14 08:20:57 UTC |
| **Commit** | 059a9f03e6b17c3fb20bf2ffe03b16c489145c1a |
| **File Size** | 85.50 MB |
| **Row Count** | 1000001 |
| **Iterations** | 5 |
| **Platform** | Linux x86_64 |

---

## Executive Summary

CISV is a high-performance CSV parser written in C with SIMD optimizations (AVX-512, AVX2, SSE2). This benchmark compares CISV against popular CSV parsing tools across different languages and use cases.

---

## 1. CLI Tools Comparison

### 1.1 Row Counting Performance

Task: Count all rows in a 85.50 MB CSV file with 1000001 rows.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.0734 | 1164.85 | 5/5 | ✓ |
| rust-csv | 0.1550 | 551.61 | 5/5 | - |
| xsv | 0.1254 | 681.82 | 5/5 | - |
| wc -l | 0.0174 | 4913.79 | 5/5 | - |
| awk | 0.1284 | 665.89 | 5/5 | - |
| miller | 0.8042 | 106.32 | 5/5 | - |
| csvkit | 2.1349 | 40.05 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3507 | 243.80 | 5/5 | ✓ |
| rust-csv | 0.2407 | 355.21 | 5/5 | - |
| xsv | 0.1719 | 497.38 | 5/5 | - |
| awk | 1.1764 | 72.68 | 5/5 | - |
| cut | 0.1855 | 460.92 | 5/5 | - |
| miller | 1.0734 | 79.65 | 5/5 | - |
| csvkit | 2.1479 | 39.81 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.4814 | 19.08 | 5/5 | ✓ |
| **cisv (count)** | 0.0687 | 1244.54 | 5/5 | ✓ |
| papaparse | 1.3906 | 61.48 | 5/5 | - |
| csv-parse | 3.7372 | 22.88 | 5/5 | - |
| fast-csv | 6.9094 | 12.37 | 5/5 | - |
| csv-parser | 2.4800 | 34.48 | 5/5 | - |
| d3-dsv | 0.8500 | 100.59 | 5/5 | - |
| csv-string | 1.3498 | 63.34 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0681 | 1255.51 | 5/5 | ✓ |
| polars | 0.0753 | 1135.46 | 5/5 | - |
| pyarrow | 0.0787 | 1086.40 | 5/5 | - |
| pandas | 1.9037 | 44.91 | 5/5 | - |
| csv (stdlib) | 1.7716 | 48.26 | 5/5 | - |
| DictReader | 2.2493 | 38.01 | 5/5 | - |
| numpy | 3.3091 | 25.84 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3941 | 216.95 | 5/5 | ✓ |
| **cisv (count)** | 0.0691 | 1237.34 | 5/5 | ✓ |
| fgetcsv | 4.9577 | 17.25 | 5/5 | - |
| str_getcsv | 4.8233 | 17.73 | 5/5 | - |
| SplFileObject | 5.3403 | 16.01 | 5/5 | - |
| league/csv | 12.9854 | 6.58 | 5/5 | - |
| explode | 0.4151 | 205.97 | 5/5 | - |
| preg_split | 0.5593 | 152.87 | 5/5 | - |
| array_map | 4.9690 | 17.21 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without PHP array creation overhead.
> cisv (parse) includes the cost of converting C data to PHP arrays.

---

## 5. Technology Notes

| Tool | Language | Key Features |
|------|----------|--------------|
| cisv | C | SIMD (AVX-512/AVX2/SSE2), zero-copy parsing |
| rust-csv | Rust | Memory-safe, streaming, Serde support |
| xsv | Rust | Full CSV toolkit, parallel processing |
| polars | Rust/Python | DataFrame, parallel, lazy evaluation |
| pyarrow | C++/Python | Apache Arrow, columnar format |
| papaparse | JavaScript | Browser/Node, streaming, auto-detect |
| league/csv | PHP | RFC 4180 compliant, streaming |

---

## Methodology

- Each test was run **5 times** and averaged
- Tests used a **85.50 MB** CSV file with **1000001** rows
- All tests ran on the same machine sequentially
- Warm-up runs were not performed (cold start)
- Times include file I/O and parsing

---

## Validation

The **Valid** column shows whether CISV correctly parsed the data:

| Symbol | Meaning |
|--------|---------|
| ✓ | Validation passed - data parsed correctly |
| ✗ | Validation failed - data mismatch detected |
| - | Not validated (third-party tool) |

### Validation Checks Performed

For each CISV binding, the following validations are performed:

1. **Row Count**: Verify the parser returns exactly **1000001** rows (including header)
2. **Field Count**: Verify each row contains **7** fields
3. **Header Verification**: Confirm headers match: `id,name,email,address,phone,date,amount`
4. **Data Integrity**: Verify first data row starts with expected ID value

### Validation Results Summary

| Binding | Count | Parse |
|---------|-------|-------|
| CLI | ✓ | ✓ |
| Node.js | ✓ | ✓ |
| Python | ✓ | ✓ |
| PHP | ✓ | ✓ |

---

*Report generated by CISV Benchmark Suite*
