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
E: Could not open lock file /var/lib/dpkg/lock-frontend - open (13: Permission denied)
E: Unable to acquire the dpkg frontend lock (/var/lib/dpkg/lock-frontend), are you root?

## CLI BENCHMARKS


### Testing with large.csv

File info: 108.19 MB, 1000001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.0211887s
  Run 2: 0.020571s
  Run 3: 0.0205362s
  Average Time: 0.0208 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.182846s
  Run 2: 0.187749s
  Run 3: 0.186468s
  Average Time: 0.1857 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0219042s
  Run 2: 0.0215757s
  Run 3: 0.0215044s
  Average Time: 0.0217 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.40991s
  Run 2: 2.30723s
  Run 3: 2.28953s
  Average Time: 2.3356 seconds
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
| cisv                 |      5201.44 |       0.0208 |          48.08 |
| wc -l                |      4985.71 |       0.0217 |          46.08 |
| rust-csv             |       582.61 |       0.1857 |           5.39 |
| csvkit               |        46.32 |       2.3356 |           0.43 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5201.44 |       0.0208 |          48.08 |
| wc -l                |      4985.71 |       0.0217 |          46.08 |
| rust-csv             |       582.61 |       0.1857 |           5.39 |
| csvkit               |        46.32 |       2.3356 |           0.43 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.455266s
  Run 2: 0.437471s
  Run 3: 0.436851s
  Average Time: 0.4432 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.339993s
  Run 2: 0.332453s
  Run 3: 0.337419s
  Average Time: 0.3366 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.55982s
  Run 2: 2.56401s
  Run 3: 2.60419s
  Average Time: 2.5760 seconds
  Successful runs: 3/3

```

=== Sorted Results: Column Selection - large.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       321.42 |       0.3366 |           2.97 |
| cisv                 |       244.11 |       0.4432 |           2.26 |
| csvkit               |        42.00 |       2.5760 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       321.42 |       0.3366 |           2.97 |
| cisv                 |       244.11 |       0.4432 |           2.26 |
| csvkit               |        42.00 |       2.5760 |           0.39 |

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
  cisv (sync) x 153,685 ops/sec ±2.00% (92 runs sampled)
    Speed: 71.08 MB/s | Avg Time: 0.01 ms | Ops/sec: 153685
    (cooling down...)

```
```
  csv-parse (sync) x 40,388 ops/sec ±1.28% (95 runs sampled)
    Speed: 18.68 MB/s | Avg Time: 0.02 ms | Ops/sec: 40388
    (cooling down...)

```
```
  papaparse (sync) x 60,782 ops/sec ±1.27% (95 runs sampled)
    Speed: 28.11 MB/s | Avg Time: 0.02 ms | Ops/sec: 60782
    (cooling down...)

```
```
  udsv (sync) x 153,891 ops/sec ±0.24% (95 runs sampled)
    Speed: 71.18 MB/s | Avg Time: 0.01 ms | Ops/sec: 153891
    (cooling down...)

```
```
  d3-dsv (sync) x 214,057 ops/sec ±0.28% (97 runs sampled)
    Speed: 99.01 MB/s | Avg Time: 0.00 ms | Ops/sec: 214057
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
```
  cisv (sync) x 227,041 ops/sec ±8.60% (70 runs sampled)
    Speed: 105.01 MB/s | Avg Time: 0.00 ms | Ops/sec: 227041
    (cooling down...)

```
```
  csv-parse (sync) x 38,901 ops/sec ±14.49% (98 runs sampled)
    Speed: 17.99 MB/s | Avg Time: 0.03 ms | Ops/sec: 38901
    (cooling down...)

```
```
  papaparse (sync) x 60,499 ops/sec ±0.72% (95 runs sampled)
    Speed: 27.98 MB/s | Avg Time: 0.02 ms | Ops/sec: 60499
    (cooling down...)

```
```
  udsv (sync) x 153,536 ops/sec ±0.33% (97 runs sampled)
    Speed: 71.02 MB/s | Avg Time: 0.01 ms | Ops/sec: 153536
    (cooling down...)

```
```
  d3-dsv (sync) x 211,573 ops/sec ±0.27% (95 runs sampled)
    Speed: 97.86 MB/s | Avg Time: 0.00 ms | Ops/sec: 211573
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
```
  cisv (async/stream) x 205,164 ops/sec ±0.89% (71 runs sampled)
    Speed: 94.89 MB/s | Avg Time: 0.00 ms | Ops/sec: 205164
    (cooling down...)

```
```
  papaparse (async/stream) x 45,668 ops/sec ±4.58% (84 runs sampled)
    Speed: 21.12 MB/s | Avg Time: 0.02 ms | Ops/sec: 45668
    (cooling down...)

```
```
  fast-csv (async/stream) x 22,090 ops/sec ±1.51% (82 runs sampled)
    Speed: 10.22 MB/s | Avg Time: 0.05 ms | Ops/sec: 22090
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,727 ops/sec ±2.36% (82 runs sampled)
    Speed: 9.59 MB/s | Avg Time: 0.05 ms | Ops/sec: 20727
    (cooling down...)

```
```
  udsv (async/stream) x 115,183 ops/sec ±0.31% (87 runs sampled)
    Speed: 53.28 MB/s | Avg Time: 0.01 ms | Ops/sec: 115183
    (cooling down...)

```

 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
```
  cisv (async/stream) x 58,903 ops/sec ±0.23% (87 runs sampled)
    Speed: 27.24 MB/s | Avg Time: 0.02 ms | Ops/sec: 58903
    (cooling down...)

```
```
  papaparse (async/stream) x 47,771 ops/sec ±2.33% (84 runs sampled)
    Speed: 22.10 MB/s | Avg Time: 0.02 ms | Ops/sec: 47771
    (cooling down...)

```
```
  fast-csv (async/stream) x 21,991 ops/sec ±0.43% (83 runs sampled)
    Speed: 10.17 MB/s | Avg Time: 0.05 ms | Ops/sec: 21991
    (cooling down...)

```
```
  neat-csv (async/promise) x 21,121 ops/sec ±1.12% (85 runs sampled)
    Speed: 9.77 MB/s | Avg Time: 0.05 ms | Ops/sec: 21121
    (cooling down...)

```
```
  udsv (async/stream) x 115,421 ops/sec ±0.80% (90 runs sampled)
    Speed: 53.39 MB/s | Avg Time: 0.01 ms | Ops/sec: 115421
    (cooling down...)

```

 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 99.01        | 0.00          | 214057         |
| udsv (sync)        | 71.18        | 0.01          | 153891         |
| cisv (sync)        | 71.08        | 0.01          | 153685         |
| papaparse (sync)   | 28.11        | 0.02          | 60782          |
| csv-parse (sync)   | 18.68        | 0.02          | 40388          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 105.01       | 0.00          | 227041         |
| d3-dsv (sync)      | 97.86        | 0.00          | 211573         |
| udsv (sync)        | 71.02        | 0.01          | 153536         |
| papaparse (sync)   | 27.98        | 0.02          | 60499          |
| csv-parse (sync)   | 17.99        | 0.03          | 38901          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 94.89        | 0.00          | 205164         |
| udsv (async/stream)      | 53.28        | 0.01          | 115183         |
| papaparse (async/stream) | 21.12        | 0.02          | 45668          |
| fast-csv (async/stream)  | 10.22        | 0.05          | 22090          |
| neat-csv (async/promise) | 9.59         | 0.05          | 20727          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 53.39        | 0.01          | 115421         |
| cisv (async/stream)      | 27.24        | 0.02          | 58903          |
| papaparse (async/stream) | 22.10        | 0.02          | 47771          |
| fast-csv (async/stream)  | 10.17        | 0.05          | 21991          |
| neat-csv (async/promise) | 9.77         | 0.05          | 21121          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 214057         | 99.01        | 0.00          |
| udsv (sync)        | 153891         | 71.18        | 0.01          |
| cisv (sync)        | 153685         | 71.08        | 0.01          |
| papaparse (sync)   | 60782          | 28.11        | 0.02          |
| csv-parse (sync)   | 40388          | 18.68        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 205164         | 94.89        | 0.00          |
| udsv (async/stream)      | 115183         | 53.28        | 0.01          |
| papaparse (async/stream) | 45668          | 21.12        | 0.02          |
| fast-csv (async/stream)  | 22090          | 10.22        | 0.05          |
| neat-csv (async/promise) | 20727          | 9.59         | 0.05          |


Cleaning up test files...
  Removed large.csv

Benchmark complete!

