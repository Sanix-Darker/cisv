# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-02 19:01:19 UTC |
| **Commit** | 6cee3dca91ee8a82c732796ea4244dc16ad4f9fc |
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
| **cisv** | 0.0144 | 5937.50 | 5/5 | ✓ |
| rust-csv | 0.1529 | 559.19 | 5/5 | - |
| xsv | 0.1208 | 707.78 | 5/5 | - |
| wc -l | 0.0169 | 5059.17 | 5/5 | - |
| awk | 0.1228 | 696.25 | 5/5 | - |
| miller | 0.7542 | 113.37 | 5/5 | - |
| csvkit | 2.0842 | 41.02 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3526 | 242.48 | 5/5 | ✓ |
| rust-csv | 0.2213 | 386.35 | 5/5 | - |
| xsv | 0.1665 | 513.51 | 5/5 | - |
| awk | 1.1482 | 74.46 | 5/5 | - |
| cut | 0.1748 | 489.13 | 5/5 | - |
| miller | 1.0731 | 79.68 | 5/5 | - |
| csvkit | 2.1585 | 39.61 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.4844 | 19.07 | 5/5 | ✓ |
| **cisv (count)** | 0.0105 | 8142.86 | 5/5 | ✓ |
| papaparse | 1.3966 | 61.22 | 5/5 | - |
| csv-parse | 3.6905 | 23.17 | 5/5 | - |
| fast-csv | 6.5175 | 13.12 | 5/5 | - |
| csv-parser | 2.4079 | 35.51 | 5/5 | - |
| d3-dsv | 0.8037 | 106.38 | 5/5 | - |
| csv-string | 1.3045 | 65.54 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0096 | 8906.25 | 5/5 | ✓ |
| polars | 0.0716 | 1194.13 | 5/5 | - |
| pyarrow | 0.0851 | 1004.70 | 5/5 | - |
| pandas | 1.4222 | 60.12 | 5/5 | - |
| csv (stdlib) | 1.6660 | 51.32 | 5/5 | - |
| DictReader | 2.2072 | 38.74 | 5/5 | - |
| numpy | 3.2829 | 26.04 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3905 | 218.95 | 5/5 | ✓ |
| **cisv (count)** | 0.0100 | 8550.00 | 5/5 | ✓ |
| fgetcsv | 4.9337 | 17.33 | 5/5 | - |
| str_getcsv | 4.8274 | 17.71 | 5/5 | - |
| SplFileObject | 5.3016 | 16.13 | 5/5 | - |
| league/csv | 12.7617 | 6.70 | 5/5 | - |
| explode | 0.4092 | 208.94 | 5/5 | - |
| preg_split | 0.5522 | 154.84 | 5/5 | - |
| array_map | 4.7563 | 17.98 | 5/5 | - |

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
| Python | ✓ | - |
| PHP | ✓ | ✓ |

---

*Report generated by CISV Benchmark Suite*
