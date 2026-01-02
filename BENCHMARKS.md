# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-02 19:14:36 UTC |
| **Commit** | 038e6b3dfcfdb4c8b0a2e3234b31d8dba6b100f8 |
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
| **cisv** | 0.0159 | 5377.36 | 5/5 | ✓ |
| rust-csv | 0.1387 | 616.44 | 5/5 | - |
| xsv | 0.1030 | 830.10 | 5/5 | - |
| wc -l | 0.0189 | 4523.81 | 5/5 | - |
| awk | 0.0994 | 860.16 | 5/5 | - |
| miller | 0.8414 | 101.62 | 5/5 | - |
| csvkit | 2.6871 | 31.82 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3175 | 269.29 | 5/5 | ✓ |
| rust-csv | 0.2039 | 419.32 | 5/5 | - |
| xsv | 0.1496 | 571.52 | 5/5 | - |
| awk | 1.0488 | 81.52 | 5/5 | - |
| cut | 0.1970 | 434.01 | 5/5 | - |
| miller | 1.2720 | 67.22 | 5/5 | - |
| csvkit | 2.1732 | 39.34 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 5.2278 | 16.35 | 5/5 | ✓ |
| **cisv (count)** | 0.0113 | 7566.37 | 5/5 | ✓ |
| papaparse | 1.6164 | 52.90 | 5/5 | - |
| csv-parse | 3.8491 | 22.21 | 5/5 | - |
| fast-csv | 7.4359 | 11.50 | 5/5 | - |
| csv-parser | 2.5170 | 33.97 | 5/5 | - |
| d3-dsv | 0.8963 | 95.39 | 5/5 | - |
| csv-string | 1.4985 | 57.06 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0111 | 7702.70 | 5/5 | ✓ |
| polars | 0.0883 | 968.29 | 5/5 | - |
| pyarrow | 0.1078 | 793.14 | 5/5 | - |
| pandas | 1.7146 | 49.87 | 5/5 | - |
| csv (stdlib) | 2.4705 | 34.61 | 5/5 | - |
| DictReader | 1.9549 | 43.74 | 5/5 | - |
| numpy | 3.1222 | 27.38 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3691 | 231.64 | 5/5 | ✓ |
| **cisv (count)** | 0.0108 | 7916.67 | 5/5 | ✓ |
| fgetcsv | 4.7036 | 18.18 | 5/5 | - |
| str_getcsv | 4.6191 | 18.51 | 5/5 | - |
| SplFileObject | 5.1316 | 16.66 | 5/5 | - |
| league/csv | 12.8754 | 6.64 | 5/5 | - |
| explode | 0.4093 | 208.89 | 5/5 | - |
| preg_split | 0.6349 | 134.67 | 5/5 | - |
| array_map | 4.6629 | 18.34 | 5/5 | - |

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
