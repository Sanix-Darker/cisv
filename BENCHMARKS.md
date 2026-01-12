# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 10:19:11 UTC |
| **Commit** | ebcd58a2ddfd05af3071bc5373080265dc8f318a |
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
| **cisv** | 0.0149 | 5738.26 | 5/5 | ✓ |
| rust-csv | 0.1540 | 555.19 | 5/5 | - |
| xsv | 0.1116 | 766.13 | 5/5 | - |
| wc -l | 0.0165 | 5181.82 | 5/5 | - |
| awk | 0.1224 | 698.53 | 5/5 | - |
| miller | 0.7560 | 113.10 | 5/5 | - |
| csvkit | 2.0623 | 41.46 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3374 | 253.41 | 5/5 | ✓ |
| rust-csv | 0.2184 | 391.48 | 5/5 | - |
| xsv | 0.1630 | 524.54 | 5/5 | - |
| awk | 1.1606 | 73.67 | 5/5 | - |
| cut | 0.1749 | 488.85 | 5/5 | - |
| miller | 1.1276 | 75.82 | 5/5 | - |
| csvkit | 2.1503 | 39.76 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.3213 | 19.79 | 5/5 | ✓ |
| **cisv (count)** | 0.0107 | 7990.65 | 5/5 | ✓ |
| papaparse | 1.3949 | 61.29 | 5/5 | - |
| csv-parse | 3.7582 | 22.75 | 5/5 | - |
| fast-csv | 6.5464 | 13.06 | 5/5 | - |
| csv-parser | 2.4081 | 35.51 | 5/5 | - |
| d3-dsv | 0.8036 | 106.40 | 5/5 | - |
| csv-string | 1.3191 | 64.82 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0099 | 8636.36 | 5/5 | ✓ |
| polars | 0.0723 | 1182.57 | 5/5 | - |
| pyarrow | 0.0856 | 998.83 | 5/5 | - |
| pandas | 1.4367 | 59.51 | 5/5 | - |
| csv (stdlib) | 1.6745 | 51.06 | 5/5 | - |
| DictReader | 2.1573 | 39.63 | 5/5 | - |
| numpy | 3.2461 | 26.34 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3648 | 234.38 | 5/5 | ✓ |
| **cisv (count)** | 0.0097 | 8814.43 | 5/5 | ✓ |
| fgetcsv | 4.9785 | 17.17 | 5/5 | - |
| str_getcsv | 4.7915 | 17.84 | 5/5 | - |
| SplFileObject | 5.2421 | 16.31 | 5/5 | - |
| league/csv | 12.9931 | 6.58 | 5/5 | - |
| explode | 0.4064 | 210.38 | 5/5 | - |
| preg_split | 0.5509 | 155.20 | 5/5 | - |
| array_map | 4.7857 | 17.87 | 5/5 | - |

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
