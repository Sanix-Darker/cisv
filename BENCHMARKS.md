## BENCHMARK RESULTS

---

Generating test CSV files...
Creating large.csv (1M rows)...
  Progress: 0%
  Progress: 10%
  Progress: 20%
  Progress: 30%
  Progress: 40%
  Progress: 50%
  Progress: 60%
  Progress: 70%
  Progress: 80%
  Progress: 90%

## CLI BENCHMARKS


### Testing with large.csv

File info: 108.19 MB, 1000001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.0233178s
  Run 2: 0.0224478s
  Run 3: 0.0222077s
  Average Time: 0.0227 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.189227s
  Run 2: 0.187593s
  Run 3: 0.187789s
  Average Time: 0.1882 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0228014s
  Run 2: 0.0229805s
  Run 3: 0.0242462s
  Average Time: 0.0233 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.53979s
  Run 2: 2.39725s
  Run 3: 2.36552s
  Average Time: 2.4342 seconds
  Successful runs: 3/3

-> miller
  Run 1 failed with exit code 127
  Run 2 failed with exit code 127
  Run 3 failed with exit code 127
  All runs failed for miller

```

=== Sorted Results: Row Counting - large.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      4766.08 |       0.0227 |          44.05 |
| wc -l                |      4643.35 |       0.0233 |          42.92 |
| rust-csv             |       574.87 |       0.1882 |           5.31 |
| csvkit               |        44.45 |       2.4342 |           0.41 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      4766.08 |       0.0227 |          44.05 |
| wc -l                |      4643.35 |       0.0233 |          42.92 |
| rust-csv             |       574.87 |       0.1882 |           5.31 |
| csvkit               |        44.45 |       2.4342 |           0.41 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.436198s
  Run 2: 0.438279s
  Run 3: 0.435271s
  Average Time: 0.4366 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.351171s
  Run 2: 0.345821s
  Run 3: 0.342545s
  Average Time: 0.3465 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.58791s
  Run 2: 2.60158s
  Run 3: 2.55235s
  Average Time: 2.5806 seconds
  Successful runs: 3/3

-> miller
  Run 1 failed with exit code 127
  Run 2 failed with exit code 127
  Run 3 failed with exit code 127
  All runs failed for miller

```

=== Sorted Results: Column Selection - large.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       312.24 |       0.3465 |           2.89 |
| cisv                 |       247.80 |       0.4366 |           2.29 |
| csvkit               |        41.92 |       2.5806 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       312.24 |       0.3465 |           2.89 |
| cisv                 |       247.80 |       0.4366 |           2.29 |
| csvkit               |        41.92 |       2.5806 |           0.39 |

## NPM Benchmarks


> cisv@0.0.7 benchmark-js
> node benchmark/benchmark.js

 Using benchmark file: /home/runner/work/cisv/cisv/fixtures/data.csv
Starting benchmark with file: /home/runner/work/cisv/cisv/fixtures/data.csv
All tests will retrieve row index: 4

File size: 0.00 MB
Sample of target row: [ '4', 'Dana White', 'dana.white@email.com', 'Chicago' ]


--- Running: Sync (Parse only) Benchmarks ---
```
  cisv (sync) x 150,683 ops/sec ±2.36% (91 runs sampled)
    Speed: 69.70 MB/s | Avg Time: 0.01 ms | Ops/sec: 150683
    (cooling down...)

```
```
  csv-parse (sync) x 40,249 ops/sec ±1.90% (95 runs sampled)
    Speed: 18.62 MB/s | Avg Time: 0.02 ms | Ops/sec: 40249
    (cooling down...)

```
```
  papaparse (sync) x 59,849 ops/sec ±1.46% (93 runs sampled)
    Speed: 27.68 MB/s | Avg Time: 0.02 ms | Ops/sec: 59849
    (cooling down...)

```
```
  udsv (sync) x 150,791 ops/sec ±0.30% (97 runs sampled)
    Speed: 69.75 MB/s | Avg Time: 0.01 ms | Ops/sec: 150791
    (cooling down...)

```
```
  d3-dsv (sync) x 208,867 ops/sec ±0.24% (99 runs sampled)
    Speed: 96.61 MB/s | Avg Time: 0.00 ms | Ops/sec: 208867
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
```
  cisv (sync) x 222,356 ops/sec ±9.22% (68 runs sampled)
    Speed: 102.85 MB/s | Avg Time: 0.00 ms | Ops/sec: 222356
    (cooling down...)

```
```
  csv-parse (sync) x 39,681 ops/sec ±3.84% (98 runs sampled)
    Speed: 18.35 MB/s | Avg Time: 0.03 ms | Ops/sec: 39681
    (cooling down...)

```
```
  papaparse (sync) x 59,232 ops/sec ±0.85% (93 runs sampled)
    Speed: 27.40 MB/s | Avg Time: 0.02 ms | Ops/sec: 59232
    (cooling down...)

```
```
  udsv (sync) x 148,844 ops/sec ±0.42% (92 runs sampled)
    Speed: 68.85 MB/s | Avg Time: 0.01 ms | Ops/sec: 148844
    (cooling down...)

```
```
  d3-dsv (sync) x 208,781 ops/sec ±0.33% (92 runs sampled)
    Speed: 96.57 MB/s | Avg Time: 0.00 ms | Ops/sec: 208781
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
```
  cisv (async/stream) x 205,172 ops/sec ±1.10% (71 runs sampled)
    Speed: 94.90 MB/s | Avg Time: 0.00 ms | Ops/sec: 205172
    (cooling down...)

```
```
  papaparse (async/stream) x 45,825 ops/sec ±3.73% (87 runs sampled)
    Speed: 21.20 MB/s | Avg Time: 0.02 ms | Ops/sec: 45825
    (cooling down...)

```
```
  fast-csv (async/stream) x 20,840 ops/sec ±1.25% (82 runs sampled)
    Speed: 9.64 MB/s | Avg Time: 0.05 ms | Ops/sec: 20840
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,039 ops/sec ±3.51% (79 runs sampled)
    Speed: 9.27 MB/s | Avg Time: 0.05 ms | Ops/sec: 20039
    (cooling down...)

```
```
  udsv (async/stream) x 114,897 ops/sec ±0.31% (87 runs sampled)
    Speed: 53.14 MB/s | Avg Time: 0.01 ms | Ops/sec: 114897
    (cooling down...)

```

 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
```
  cisv (async/stream) x 58,408 ops/sec ±0.38% (82 runs sampled)
    Speed: 27.02 MB/s | Avg Time: 0.02 ms | Ops/sec: 58408
    (cooling down...)

```
```
  papaparse (async/stream) x 48,128 ops/sec ±2.10% (88 runs sampled)
    Speed: 22.26 MB/s | Avg Time: 0.02 ms | Ops/sec: 48128
    (cooling down...)

```
```
  fast-csv (async/stream) x 21,430 ops/sec ±0.34% (87 runs sampled)
    Speed: 9.91 MB/s | Avg Time: 0.05 ms | Ops/sec: 21430
    (cooling down...)

```
```
  neat-csv (async/promise) x 21,035 ops/sec ±1.06% (84 runs sampled)
    Speed: 9.73 MB/s | Avg Time: 0.05 ms | Ops/sec: 21035
    (cooling down...)

```
```
  udsv (async/stream) x 115,525 ops/sec ±0.21% (90 runs sampled)
    Speed: 53.43 MB/s | Avg Time: 0.01 ms | Ops/sec: 115525
    (cooling down...)

```

 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 96.61        | 0.00          | 208867         |
| udsv (sync)        | 69.75        | 0.01          | 150791         |
| cisv (sync)        | 69.70        | 0.01          | 150683         |
| papaparse (sync)   | 27.68        | 0.02          | 59849          |
| csv-parse (sync)   | 18.62        | 0.02          | 40249          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 102.85       | 0.00          | 222356         |
| d3-dsv (sync)      | 96.57        | 0.00          | 208781         |
| udsv (sync)        | 68.85        | 0.01          | 148844         |
| papaparse (sync)   | 27.40        | 0.02          | 59232          |
| csv-parse (sync)   | 18.35        | 0.03          | 39681          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 94.90        | 0.00          | 205172         |
| udsv (async/stream)      | 53.14        | 0.01          | 114897         |
| papaparse (async/stream) | 21.20        | 0.02          | 45825          |
| fast-csv (async/stream)  | 9.64         | 0.05          | 20840          |
| neat-csv (async/promise) | 9.27         | 0.05          | 20039          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 53.43        | 0.01          | 115525         |
| cisv (async/stream)      | 27.02        | 0.02          | 58408          |
| papaparse (async/stream) | 22.26        | 0.02          | 48128          |
| fast-csv (async/stream)  | 9.91         | 0.05          | 21430          |
| neat-csv (async/promise) | 9.73         | 0.05          | 21035          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 208867         | 96.61        | 0.00          |
| udsv (sync)        | 150791         | 69.75        | 0.01          |
| cisv (sync)        | 150683         | 69.70        | 0.01          |
| papaparse (sync)   | 59849          | 27.68        | 0.02          |
| csv-parse (sync)   | 40249          | 18.62        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 205172         | 94.90        | 0.00          |
| udsv (async/stream)      | 114897         | 53.14        | 0.01          |
| papaparse (async/stream) | 45825          | 21.20        | 0.02          |
| fast-csv (async/stream)  | 20840          | 9.64         | 0.05          |
| neat-csv (async/promise) | 20039          | 9.27         | 0.05          |


Cleaning up test files...
Removed large.csv

Benchmark complete!

