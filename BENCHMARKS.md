Running CLI benchmarks...
Running Node.js benchmarks...

make: Entering directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
  CC(target) Release/obj.target/nothing/node_modules/node-addon-api/nothing.o
rm -f Release/obj.target/node_modules/node-addon-api/nothing.a Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list; mkdir -p `dirname Release/obj.target/node_modules/node-addon-api/nothing.a`
ar crs Release/obj.target/node_modules/node-addon-api/nothing.a @Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list
  COPY Release/nothing.a
  CXX(target) Release/obj.target/cisv/cisv/cisv_addon.o
  CC(target) Release/obj.target/cisv/../../core/src/parser.o
  CC(target) Release/obj.target/cisv/../../core/src/writer.o
  CC(target) Release/obj.target/cisv/../../core/src/transformer.o
  SOLINK_MODULE(target) Release/obj.target/cisv.node
  COPY Release/cisv.node
make: Leaving directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'

make: Entering directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
  CC(target) Release/obj.target/nothing/node_modules/node-addon-api/nothing.o
rm -f Release/obj.target/node_modules/node-addon-api/nothing.a Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list; mkdir -p `dirname Release/obj.target/node_modules/node-addon-api/nothing.a`
ar crs Release/obj.target/node_modules/node-addon-api/nothing.a @Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list
  COPY Release/nothing.a
  CXX(target) Release/obj.target/cisv/cisv/cisv_addon.o
  CC(target) Release/obj.target/cisv/../../core/src/parser.o
  CC(target) Release/obj.target/cisv/../../core/src/writer.o
  CC(target) Release/obj.target/cisv/../../core/src/transformer.o
  SOLINK_MODULE(target) Release/obj.target/cisv.node
  COPY Release/cisv.node
make: Leaving directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
Running Python benchmarks...
# CISV Benchmark Report

> **Generated:** 2026-01-02 15:51:12 UTC
> **Commit:** 38b0135297d6edbfd177c2f8b5925632f629a90d
> **Test File:** 85.50 MB, 1000001 rows

## Summary

CISV is a high-performance CSV parser with SIMD optimizations. This benchmark compares it against other popular CSV tools.

---

## CLI Benchmarks

### Row Counting

Counting all rows in the CSV file.

| Tool | Time (s) | Speed (MB/s) | Status |
|------|----------|--------------|--------|
| cisv | 0.0149 | 5738.26 | OK (3/3) |
| rust-csv | 0.1683 | 508.02 | OK (3/3) |
| wc -l | 0.0161 | 5310.56 | OK (3/3) |
| csvkit | 2.0654 | 41.40 | OK (3/3) |
| miller | 0.7497 | 114.05 | OK (3/3) |

### Column Selection

Selecting columns 0, 2, 3 from the CSV file.

| Tool | Time (s) | Speed (MB/s) | Status |
|------|----------|--------------|--------|
| cisv | 0.3420 | 250.00 | OK (3/3) |
| rust-csv | 0.2222 | 384.79 | OK (3/3) |
| csvkit | 2.1556 | 39.66 | OK (3/3) |
| miller | 1.0618 | 80.52 | OK (3/3) |

---

## Node.js Binding Benchmarks

Parsing the same CSV file using the Node.js binding.

| Parser | Time (s) | Speed (MB/s) | Status |
|--------|----------|--------------|--------|
| cisv-node | - | - | Not tested |
| papaparse | - | - | Not tested |
| csv-parse | - | - | Not tested |
| fast-csv | - | - | Not tested |

---

## Python Binding Benchmarks

Parsing the same CSV file using the Python binding.

| Parser | Time (s) | Speed (MB/s) | Status |
|--------|----------|--------------|--------|
| cisv-python | - | - | Not tested |
| pandas | 1.5118 | 56.56 | OK |
| csv-stdlib | 1.6430 | 52.04 | OK |

---

## Performance Analysis

### Ranking by Speed (Row Counting)

1. **cisv** - 5738.26 MB/s
2. **rust-csv** - 508.02 MB/s
3. **wc -l** - 5310.56 MB/s
4. **csvkit** - 41.40 MB/s
5. **miller** - 114.05 MB/s

### Notes

- **cisv**: Native C implementation with SIMD optimizations (AVX-512/AVX2/SSE2)
- **rust-csv**: Rust CSV library with optimized parsing
- **wc -l**: Simple line counting (no CSV parsing)
- **csvkit**: Python-based CSV toolkit
- **miller**: Feature-rich data processing tool

---

*Benchmark conducted with 3 iterations per test.*
Benchmark complete!
