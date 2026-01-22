# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-22 09:53:39 UTC |
| **Commit** | 48f788c3ecc23b83e6c97108aa68f9e8c5110ae2 |
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
| rust-csv | 0.1545 | 553.40 | 5/5 | - |
| xsv | 0.1213 | 704.86 | 5/5 | - |
| wc -l | 0.0157 | 5445.86 | 5/5 | - |
| awk | 0.1216 | 703.12 | 5/5 | - |
| miller | 0.8054 | 106.16 | 5/5 | - |
| csvkit | 2.0783 | 41.14 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3444 | 248.26 | 5/5 | ✓ |
| rust-csv | 0.2215 | 386.00 | 5/5 | - |
| xsv | 0.1817 | 470.56 | 5/5 | - |
| awk | 1.1583 | 73.82 | 5/5 | - |
| cut | 0.1750 | 488.57 | 5/5 | - |
| miller | 1.0840 | 78.87 | 5/5 | - |
| csvkit | 2.1734 | 39.34 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.6422 | 18.42 | 5/5 | ✓ |
| **cisv (count)** | 0.0110 | 7772.73 | 5/5 | ✓ |
| papaparse | 1.4017 | 61.00 | 5/5 | - |
| csv-parse | 3.7460 | 22.82 | 5/5 | - |
| fast-csv | 6.8099 | 12.56 | 5/5 | - |
| csv-parser | 2.4474 | 34.94 | 5/5 | - |
| d3-dsv | 0.8218 | 104.04 | 5/5 | - |
| csv-string | 1.3754 | 62.16 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0105 | 8142.86 | 5/5 | ✓ |
| polars | 0.0753 | 1135.46 | 5/5 | - |
| pyarrow | 0.0810 | 1055.56 | 5/5 | - |
| pandas | 2.1177 | 40.37 | 5/5 | - |
| csv (stdlib) | 1.7676 | 48.37 | 5/5 | - |
| DictReader | 2.1923 | 39.00 | 5/5 | - |
| numpy | 3.2472 | 26.33 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.4161 | 205.48 | 5/5 | ✓ |
| **cisv (count)** | 0.0113 | 7566.37 | 5/5 | ✓ |
| fgetcsv | 5.0201 | 17.03 | 5/5 | - |
| str_getcsv | 4.8645 | 17.58 | 5/5 | - |
| SplFileObject | 5.3107 | 16.10 | 5/5 | - |
| league/csv | 12.7489 | 6.71 | 5/5 | - |
| explode | 0.4187 | 204.20 | 5/5 | - |
| preg_split | 0.5594 | 152.84 | 5/5 | - |
| array_map | 4.8145 | 17.76 | 5/5 | - |

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
