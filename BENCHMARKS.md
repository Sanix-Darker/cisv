# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 13:19:44 UTC |
| **Commit** | bb2f4c40ff65bb50bf3538f51eb10418217f81c7 |
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
| rust-csv | 0.1389 | 615.55 | 5/5 | - |
| xsv | 0.0998 | 856.71 | 5/5 | - |
| wc -l | 0.0179 | 4776.54 | 5/5 | - |
| awk | 0.0987 | 866.26 | 5/5 | - |
| miller | 0.8108 | 105.45 | 5/5 | - |
| csvkit | 2.5755 | 33.20 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3268 | 261.63 | 5/5 | ✓ |
| rust-csv | 0.2038 | 419.53 | 5/5 | - |
| xsv | 0.1498 | 570.76 | 5/5 | - |
| awk | 1.0455 | 81.78 | 5/5 | - |
| cut | 0.1978 | 432.25 | 5/5 | - |
| miller | 1.1943 | 71.59 | 5/5 | - |
| csvkit | 2.1510 | 39.75 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.8426 | 17.66 | 5/5 | ✓ |
| **cisv (count)** | 0.0106 | 8066.04 | 5/5 | ✓ |
| papaparse | 1.5451 | 55.34 | 5/5 | - |
| csv-parse | 3.7341 | 22.90 | 5/5 | - |
| fast-csv | 7.0054 | 12.20 | 5/5 | - |
| csv-parser | 2.4381 | 35.07 | 5/5 | - |
| d3-dsv | 0.8655 | 98.79 | 5/5 | - |
| csv-string | 1.4225 | 60.11 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0100 | 8550.00 | 5/5 | ✓ |
| polars | 0.0899 | 951.06 | 5/5 | - |
| pyarrow | 0.1059 | 807.37 | 5/5 | - |
| pandas | 1.4158 | 60.39 | 5/5 | - |
| csv (stdlib) | 2.3357 | 36.61 | 5/5 | - |
| DictReader | 1.9407 | 44.06 | 5/5 | - |
| numpy | 3.0939 | 27.64 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3686 | 231.96 | 5/5 | ✓ |
| **cisv (count)** | 0.0102 | 8382.35 | 5/5 | ✓ |
| fgetcsv | 4.7776 | 17.90 | 5/5 | - |
| str_getcsv | 4.6982 | 18.20 | 5/5 | - |
| SplFileObject | 5.1744 | 16.52 | 5/5 | - |
| league/csv | 12.6074 | 6.78 | 5/5 | - |
| explode | 0.4411 | 193.83 | 5/5 | - |
| preg_split | 0.5698 | 150.05 | 5/5 | - |
| array_map | 4.6410 | 18.42 | 5/5 | - |

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
