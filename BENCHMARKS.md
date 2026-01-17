# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-17 09:04:46 UTC |
| **Commit** | 99d76eccab788cbbfa06490b314473b391b8ba8b |
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
| **cisv** | 0.0145 | 5896.55 | 5/5 | ✓ |
| rust-csv | 0.1535 | 557.00 | 5/5 | - |
| xsv | 0.1113 | 768.19 | 5/5 | - |
| wc -l | 0.0174 | 4913.79 | 5/5 | - |
| awk | 0.1215 | 703.70 | 5/5 | - |
| miller | 0.7566 | 113.01 | 5/5 | - |
| csvkit | 2.0611 | 41.48 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3530 | 242.21 | 5/5 | ✓ |
| rust-csv | 0.2195 | 389.52 | 5/5 | - |
| xsv | 0.1608 | 531.72 | 5/5 | - |
| awk | 1.1478 | 74.49 | 5/5 | - |
| cut | 0.1754 | 487.46 | 5/5 | - |
| miller | 1.1065 | 77.27 | 5/5 | - |
| csvkit | 2.1756 | 39.30 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.4388 | 19.26 | 5/5 | ✓ |
| **cisv (count)** | 0.0103 | 8300.97 | 5/5 | ✓ |
| papaparse | 1.3742 | 62.22 | 5/5 | - |
| csv-parse | 3.7090 | 23.05 | 5/5 | - |
| fast-csv | 6.6119 | 12.93 | 5/5 | - |
| csv-parser | 2.4131 | 35.43 | 5/5 | - |
| d3-dsv | 0.8098 | 105.58 | 5/5 | - |
| csv-string | 1.3116 | 65.19 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0097 | 8814.43 | 5/5 | ✓ |
| polars | 0.0742 | 1152.29 | 5/5 | - |
| pyarrow | 0.0850 | 1005.88 | 5/5 | - |
| pandas | 1.5391 | 55.55 | 5/5 | - |
| csv (stdlib) | 1.6806 | 50.87 | 5/5 | - |
| DictReader | 2.1805 | 39.21 | 5/5 | - |
| numpy | 3.1648 | 27.02 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3813 | 224.23 | 5/5 | ✓ |
| **cisv (count)** | 0.0097 | 8814.43 | 5/5 | ✓ |
| fgetcsv | 4.9399 | 17.31 | 5/5 | - |
| str_getcsv | 4.8687 | 17.56 | 5/5 | - |
| SplFileObject | 5.3116 | 16.10 | 5/5 | - |
| league/csv | 12.8129 | 6.67 | 5/5 | - |
| explode | 0.4084 | 209.35 | 5/5 | - |
| preg_split | 0.5495 | 155.60 | 5/5 | - |
| array_map | 4.8165 | 17.75 | 5/5 | - |

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
