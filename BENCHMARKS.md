# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 08:21:04 UTC |
| **Commit** | b8f963fcef9fcd7e30ceedb2d1e39d8b82969242 |
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
| **cisv** | 0.0148 | 5777.03 | 5/5 | ✓ |
| rust-csv | 0.1528 | 559.55 | 5/5 | - |
| xsv | 0.1104 | 774.46 | 5/5 | - |
| wc -l | 0.0159 | 5377.36 | 5/5 | - |
| awk | 0.1223 | 699.10 | 5/5 | - |
| miller | 0.7792 | 109.73 | 5/5 | - |
| csvkit | 2.0729 | 41.25 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3499 | 244.36 | 5/5 | ✓ |
| rust-csv | 0.2217 | 385.66 | 5/5 | - |
| xsv | 0.1617 | 528.76 | 5/5 | - |
| awk | 1.1501 | 74.34 | 5/5 | - |
| cut | 0.1742 | 490.82 | 5/5 | - |
| miller | 1.0779 | 79.32 | 5/5 | - |
| csvkit | 2.1413 | 39.93 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.4818 | 19.08 | 5/5 | ✓ |
| **cisv (count)** | 0.0107 | 7990.65 | 5/5 | ✓ |
| papaparse | 1.3632 | 62.72 | 5/5 | - |
| csv-parse | 3.7185 | 22.99 | 5/5 | - |
| fast-csv | 6.6321 | 12.89 | 5/5 | - |
| csv-parser | 2.4275 | 35.22 | 5/5 | - |
| d3-dsv | 0.8132 | 105.14 | 5/5 | - |
| csv-string | 1.3256 | 64.50 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0102 | 8382.35 | 5/5 | ✓ |
| polars | 0.0717 | 1192.47 | 5/5 | - |
| pyarrow | 0.0866 | 987.30 | 5/5 | - |
| pandas | 1.5556 | 54.96 | 5/5 | - |
| csv (stdlib) | 1.6972 | 50.38 | 5/5 | - |
| DictReader | 2.3261 | 36.76 | 5/5 | - |
| numpy | 3.1545 | 27.10 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3792 | 225.47 | 5/5 | ✓ |
| **cisv (count)** | 0.0101 | 8465.35 | 5/5 | ✓ |
| fgetcsv | 4.8920 | 17.48 | 5/5 | - |
| str_getcsv | 4.8150 | 17.76 | 5/5 | - |
| SplFileObject | 5.2700 | 16.22 | 5/5 | - |
| league/csv | 12.7998 | 6.68 | 5/5 | - |
| explode | 0.4048 | 211.22 | 5/5 | - |
| preg_split | 0.5496 | 155.57 | 5/5 | - |
| array_map | 4.8015 | 17.81 | 5/5 | - |

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
