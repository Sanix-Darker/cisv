# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 16:15:21 UTC |
| **Commit** | 1fbed2ad4ecf105785758e1dc800273c86080f1d |
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
| **cisv** | 0.0147 | 5816.33 | 5/5 | ✓ |
| rust-csv | 0.1524 | 561.02 | 5/5 | - |
| xsv | 0.1097 | 779.40 | 5/5 | - |
| wc -l | 0.0149 | 5738.26 | 5/5 | - |
| awk | 0.1214 | 704.28 | 5/5 | - |
| miller | 0.7660 | 111.62 | 5/5 | - |
| csvkit | 2.0509 | 41.69 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3558 | 240.30 | 5/5 | ✓ |
| rust-csv | 0.2200 | 388.64 | 5/5 | - |
| xsv | 0.1613 | 530.07 | 5/5 | - |
| awk | 1.1456 | 74.63 | 5/5 | - |
| cut | 0.1752 | 488.01 | 5/5 | - |
| miller | 1.0718 | 79.77 | 5/5 | - |
| csvkit | 2.1446 | 39.87 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.5817 | 18.66 | 5/5 | ✓ |
| **cisv (count)** | 0.0105 | 8142.86 | 5/5 | ✓ |
| papaparse | 1.3517 | 63.25 | 5/5 | - |
| csv-parse | 3.7567 | 22.76 | 5/5 | - |
| fast-csv | 6.5036 | 13.15 | 5/5 | - |
| csv-parser | 2.4283 | 35.21 | 5/5 | - |
| d3-dsv | 0.8170 | 104.65 | 5/5 | - |
| csv-string | 1.3378 | 63.91 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0100 | 8550.00 | 5/5 | ✓ |
| polars | 0.0726 | 1177.69 | 5/5 | - |
| pyarrow | 0.0870 | 982.76 | 5/5 | - |
| pandas | 1.4871 | 57.49 | 5/5 | - |
| csv (stdlib) | 1.6982 | 50.35 | 5/5 | - |
| DictReader | 2.1960 | 38.93 | 5/5 | - |
| numpy | 3.1871 | 26.83 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.3867 | 221.10 | 5/5 | ✓ |
| **cisv (count)** | 0.0098 | 8724.49 | 5/5 | ✓ |
| fgetcsv | 4.8987 | 17.45 | 5/5 | - |
| str_getcsv | 4.8420 | 17.66 | 5/5 | - |
| SplFileObject | 5.3278 | 16.05 | 5/5 | - |
| league/csv | 12.8834 | 6.64 | 5/5 | - |
| explode | 0.4005 | 213.48 | 5/5 | - |
| preg_split | 0.5457 | 156.68 | 5/5 | - |
| array_map | 4.7731 | 17.91 | 5/5 | - |

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
