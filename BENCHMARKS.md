# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 12:16:55 UTC |
| **Commit** | db29543fda57cd7b16ea457e35e91beb2c5645a1 |
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
| **cisv** | 0.0150 | 5700.00 | 5/5 | ✓ |
| rust-csv | 0.1534 | 557.37 | 5/5 | - |
| xsv | 0.1108 | 771.66 | 5/5 | - |
| wc -l | 0.0171 | 5000.00 | 5/5 | - |
| awk | 0.1219 | 701.39 | 5/5 | - |
| miller | 0.8171 | 104.64 | 5/5 | - |
| csvkit | 2.0586 | 41.53 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3436 | 248.84 | 5/5 | ✓ |
| rust-csv | 0.2203 | 388.11 | 5/5 | - |
| xsv | 0.1617 | 528.76 | 5/5 | - |
| awk | 1.1518 | 74.23 | 5/5 | - |
| cut | 0.1744 | 490.25 | 5/5 | - |
| miller | 1.0787 | 79.26 | 5/5 | - |
| csvkit | 2.1564 | 39.65 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.3051 | 19.86 | 5/5 | ✓ |
| **cisv (count)** | 0.0110 | 7772.73 | 5/5 | ✓ |
| papaparse | 1.3608 | 62.83 | 5/5 | - |
| csv-parse | 3.7545 | 22.77 | 5/5 | - |
| fast-csv | 6.7915 | 12.59 | 5/5 | - |
| csv-parser | 2.4518 | 34.87 | 5/5 | - |
| d3-dsv | 0.8228 | 103.91 | 5/5 | - |
| csv-string | 1.3338 | 64.10 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0098 | 8724.49 | 5/5 | ✓ |
| polars | 0.0719 | 1189.15 | 5/5 | - |
| pyarrow | 0.0850 | 1005.88 | 5/5 | - |
| pandas | 1.6487 | 51.86 | 5/5 | - |
| csv (stdlib) | 1.6812 | 50.86 | 5/5 | - |
| DictReader | 2.1566 | 39.65 | 5/5 | - |
| numpy | 3.2795 | 26.07 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3784 | 225.95 | 5/5 | ✓ |
| **cisv (count)** | 0.0099 | 8636.36 | 5/5 | ✓ |
| fgetcsv | 4.9127 | 17.40 | 5/5 | - |
| str_getcsv | 4.7623 | 17.95 | 5/5 | - |
| SplFileObject | 5.3542 | 15.97 | 5/5 | - |
| league/csv | 12.6930 | 6.74 | 5/5 | - |
| explode | 0.4031 | 212.11 | 5/5 | - |
| preg_split | 0.5474 | 156.19 | 5/5 | - |
| array_map | 4.7965 | 17.83 | 5/5 | - |

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
