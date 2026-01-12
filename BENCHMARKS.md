# CISV Benchmark Report

## Test Configuration

| Parameter | Value |
|-----------|-------|
| **Generated** | 2026-01-12 09:51:18 UTC |
| **Commit** | fe7a65d2a811de577f6241e52a4d824db420e464 |
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
| **cisv** | 0.0156 | 5480.77 | 5/5 | ✓ |
| rust-csv | 0.1539 | 555.56 | 5/5 | - |
| xsv | 0.1127 | 758.65 | 5/5 | - |
| wc -l | 0.0173 | 4942.20 | 5/5 | - |
| awk | 0.1220 | 700.82 | 5/5 | - |
| miller | 0.7952 | 107.52 | 5/5 | - |
| csvkit | 2.1072 | 40.58 | 5/5 | - |

### 1.2 Column Selection Performance

Task: Select columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Runs | Valid |
|------|----------|--------------|------|-------|
| **cisv** | 0.3430 | 249.27 | 5/5 | ✓ |
| rust-csv | 0.2246 | 380.68 | 5/5 | - |
| xsv | 0.1651 | 517.87 | 5/5 | - |
| awk | 1.1531 | 74.15 | 5/5 | - |
| cut | 0.1799 | 475.26 | 5/5 | - |
| miller | 1.2252 | 69.78 | 5/5 | - |
| csvkit | 2.1646 | 39.50 | 5/5 | - |

---

## 2. Node.js Binding Comparison

Task: Parse the entire CSV file using Node.js parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 4.3824 | 19.51 | 5/5 | ✓ |
| **cisv (count)** | 0.0107 | 7990.65 | 5/5 | ✓ |
| papaparse | 1.4148 | 60.43 | 5/5 | - |
| csv-parse | 3.7783 | 22.63 | 5/5 | - |
| fast-csv | 7.1510 | 11.96 | 5/5 | - |
| csv-parser | 2.5573 | 33.43 | 5/5 | - |
| d3-dsv | 0.8687 | 98.42 | 5/5 | - |
| csv-string | 1.4326 | 59.68 | 5/5 | - |

> **Note:** cisv (count) shows native C performance without JS object creation overhead.
> cisv (parse) includes the cost of converting C data to JavaScript arrays.

---

## 3. Python Binding Comparison

Task: Parse the entire CSV file using Python parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv** | 0.0098 | 8724.49 | 5/5 | ✓ |
| polars | 0.0731 | 1169.63 | 5/5 | - |
| pyarrow | 0.0859 | 995.34 | 5/5 | - |
| pandas | 1.9793 | 43.20 | 5/5 | - |
| csv (stdlib) | 1.7073 | 50.08 | 5/5 | - |
| DictReader | 2.2228 | 38.46 | 5/5 | - |
| numpy | 3.2994 | 25.91 | 5/5 | - |

---

## 4. PHP Binding Comparison

Task: Parse the entire CSV file using PHP parsers.

| Parser | Time (s) | Speed (MB/s) | Runs | Valid |
|--------|----------|--------------|------|-------|
| **cisv (parse)** | 0.4758 | 179.70 | 5/5 | ✓ |
| **cisv (count)** | 0.0128 | 6679.69 | 5/5 | ✓ |
| fgetcsv | 4.9127 | 17.40 | 5/5 | - |
| str_getcsv | 4.8798 | 17.52 | 5/5 | - |
| SplFileObject | 5.3286 | 16.05 | 5/5 | - |
| league/csv | 12.8903 | 6.63 | 5/5 | - |
| explode | 0.4067 | 210.23 | 5/5 | - |
| preg_split | 0.5742 | 148.90 | 5/5 | - |
| array_map | 4.7559 | 17.98 | 5/5 | - |

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
