## BENCHMARK RESULTS
**DATE:** Thu Oct 16 23:54:04 UTC 2025
| **COMMIT:** ac7c6deae6d3c08c3d0a5dc6695de16e0b13cf23
---


Generating test CSV files...
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
  Progress: 0%

## CLI BENCHMARKS


### Testing with small.csv

File info: 0.09 MB, 1001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.00852633s
  Run 2: 0.00787115s
  Run 3: 0.00783372s
  Average Time: 0.0081 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.00836968s
  Run 2: 0.00827384s
  Run 3: 0.00843263s
  Average Time: 0.0084 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0082314s
  Run 2: 0.00808573s
  Run 3: 0.00811124s
  Average Time: 0.0081 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.212403s
  Run 2: 0.115982s
  Run 3: 0.118929s
  Average Time: 0.1491 seconds
  Successful runs: 3/3

```

=== Sorted Results: Row Counting - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        11.11 |       0.0081 |         123.46 |
| cisv                 |        11.11 |       0.0081 |         123.46 |
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.60 |       0.1491 |           6.71 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        11.11 |       0.0081 |         123.46 |
| cisv                 |        11.11 |       0.0081 |         123.46 |
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.60 |       0.1491 |           6.71 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.00833344s
  Run 2: 0.00833631s
  Run 3: 0.00824928s
  Average Time: 0.0083 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.00828671s
  Run 2: 0.00840092s
  Run 3: 0.00835514s
  Average Time: 0.0083 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.116s
  Run 2: 0.117578s
  Run 3: 0.119571s
  Average Time: 0.1177 seconds
  Successful runs: 3/3

```

=== Sorted Results: Column Selection - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.84 |       0.0083 |         120.48 |
| cisv                 |        10.84 |       0.0083 |         120.48 |
| csvkit               |         0.76 |       0.1177 |           8.50 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.84 |       0.0083 |         120.48 |
| cisv                 |        10.84 |       0.0083 |         120.48 |
| csvkit               |         0.76 |       0.1177 |           8.50 |

### Testing with medium.csv

File info: 10.25 MB, 100001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.0092144s
  Run 2: 0.00908828s
  Run 3: 0.00924778s
  Average Time: 0.0092 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.0257616s
  Run 2: 0.0260634s
  Run 3: 0.0260463s
  Average Time: 0.0260 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.00982976s
  Run 2: 0.00974226s
  Run 3: 0.00976634s
  Average Time: 0.0098 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.28457s
  Run 2: 0.282212s
  Run 3: 0.284944s
  Average Time: 0.2839 seconds
  Successful runs: 3/3

```

=== Sorted Results: Row Counting - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1114.13 |       0.0092 |         108.70 |
| wc -l                |      1045.92 |       0.0098 |         102.04 |
| rust-csv             |       394.23 |       0.0260 |          38.46 |
| csvkit               |        36.10 |       0.2839 |           3.52 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1114.13 |       0.0092 |         108.70 |
| wc -l                |      1045.92 |       0.0098 |         102.04 |
| rust-csv             |       394.23 |       0.0260 |          38.46 |
| csvkit               |        36.10 |       0.2839 |           3.52 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.0533195s
  Run 2: 0.0534384s
  Run 3: 0.051544s
  Average Time: 0.0528 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.0396314s
  Run 2: 0.0398347s
  Run 3: 0.0392864s
  Average Time: 0.0396 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.358168s
  Run 2: 0.357236s
  Run 3: 0.355257s
  Average Time: 0.3569 seconds
  Successful runs: 3/3

```

=== Sorted Results: Column Selection - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       258.84 |       0.0396 |          25.25 |
| cisv                 |       194.13 |       0.0528 |          18.94 |
| csvkit               |        28.72 |       0.3569 |           2.80 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       258.84 |       0.0396 |          25.25 |
| cisv                 |       194.13 |       0.0528 |          18.94 |
| csvkit               |        28.72 |       0.3569 |           2.80 |

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
  cisv (sync) x 154,323 ops/sec ±2.01% (92 runs sampled)
    Speed: 71.38 MB/s | Avg Time: 0.01 ms | Ops/sec: 154323
    (cooling down...)

```
```
  csv-parse (sync) x 40,718 ops/sec ±1.37% (96 runs sampled)
    Speed: 18.83 MB/s | Avg Time: 0.02 ms | Ops/sec: 40718
    (cooling down...)

```
```
  papaparse (sync) x 60,707 ops/sec ±1.19% (95 runs sampled)
    Speed: 28.08 MB/s | Avg Time: 0.02 ms | Ops/sec: 60707
    (cooling down...)

```
```
  udsv (sync) x 151,442 ops/sec ±0.36% (98 runs sampled)
    Speed: 70.05 MB/s | Avg Time: 0.01 ms | Ops/sec: 151442
    (cooling down...)

```
```
  d3-dsv (sync) x 212,460 ops/sec ±0.35% (98 runs sampled)
    Speed: 98.27 MB/s | Avg Time: 0.00 ms | Ops/sec: 212460
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
```
  cisv (sync) x 225,941 ops/sec ±9.08% (69 runs sampled)
    Speed: 104.50 MB/s | Avg Time: 0.00 ms | Ops/sec: 225941
    (cooling down...)

```
```
  csv-parse (sync) x 42,732 ops/sec ±0.15% (98 runs sampled)
    Speed: 19.76 MB/s | Avg Time: 0.02 ms | Ops/sec: 42732
    (cooling down...)

```
```
  papaparse (sync) x 61,275 ops/sec ±0.85% (96 runs sampled)
    Speed: 28.34 MB/s | Avg Time: 0.02 ms | Ops/sec: 61275
    (cooling down...)

```
```
  udsv (sync) x 151,548 ops/sec ±0.47% (95 runs sampled)
    Speed: 70.10 MB/s | Avg Time: 0.01 ms | Ops/sec: 151548
    (cooling down...)

```
```
  d3-dsv (sync) x 211,125 ops/sec ±0.29% (97 runs sampled)
    Speed: 97.65 MB/s | Avg Time: 0.00 ms | Ops/sec: 211125
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
```
  cisv (async/stream) x 215,118 ops/sec ±0.73% (71 runs sampled)
    Speed: 99.50 MB/s | Avg Time: 0.00 ms | Ops/sec: 215118
    (cooling down...)

```
```
  papaparse (async/stream) x 45,538 ops/sec ±3.76% (79 runs sampled)
    Speed: 21.06 MB/s | Avg Time: 0.02 ms | Ops/sec: 45538
    (cooling down...)

```
```
  fast-csv (async/stream) x 22,387 ops/sec ±0.99% (87 runs sampled)
    Speed: 10.35 MB/s | Avg Time: 0.04 ms | Ops/sec: 22387
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,314 ops/sec ±2.24% (82 runs sampled)
    Speed: 9.40 MB/s | Avg Time: 0.05 ms | Ops/sec: 20314
    (cooling down...)

```
```
  udsv (async/stream) x 113,115 ops/sec ±0.28% (90 runs sampled)
    Speed: 52.32 MB/s | Avg Time: 0.01 ms | Ops/sec: 113115
    (cooling down...)

```

 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
```
  cisv (async/stream) x 59,077 ops/sec ±0.24% (88 runs sampled)
    Speed: 27.32 MB/s | Avg Time: 0.02 ms | Ops/sec: 59077
    (cooling down...)

```
```
  papaparse (async/stream) x 48,661 ops/sec ±2.00% (85 runs sampled)
    Speed: 22.51 MB/s | Avg Time: 0.02 ms | Ops/sec: 48661
    (cooling down...)

```
```
  fast-csv (async/stream) x 22,013 ops/sec ±0.48% (85 runs sampled)
    Speed: 10.18 MB/s | Avg Time: 0.05 ms | Ops/sec: 22013
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,947 ops/sec ±1.46% (87 runs sampled)
    Speed: 9.69 MB/s | Avg Time: 0.05 ms | Ops/sec: 20947
    (cooling down...)

```
```
  udsv (async/stream) x 113,565 ops/sec ±0.40% (91 runs sampled)
    Speed: 52.53 MB/s | Avg Time: 0.01 ms | Ops/sec: 113565
    (cooling down...)

```

 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 98.27        | 0.00          | 212460         |
| cisv (sync)        | 71.38        | 0.01          | 154323         |
| udsv (sync)        | 70.05        | 0.01          | 151442         |
| papaparse (sync)   | 28.08        | 0.02          | 60707          |
| csv-parse (sync)   | 18.83        | 0.02          | 40718          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 104.50       | 0.00          | 225941         |
| d3-dsv (sync)      | 97.65        | 0.00          | 211125         |
| udsv (sync)        | 70.10        | 0.01          | 151548         |
| papaparse (sync)   | 28.34        | 0.02          | 61275          |
| csv-parse (sync)   | 19.76        | 0.02          | 42732          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 99.50        | 0.00          | 215118         |
| udsv (async/stream)      | 52.32        | 0.01          | 113115         |
| papaparse (async/stream) | 21.06        | 0.02          | 45538          |
| fast-csv (async/stream)  | 10.35        | 0.04          | 22387          |
| neat-csv (async/promise) | 9.40         | 0.05          | 20314          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 52.53        | 0.01          | 113565         |
| cisv (async/stream)      | 27.32        | 0.02          | 59077          |
| papaparse (async/stream) | 22.51        | 0.02          | 48661          |
| fast-csv (async/stream)  | 10.18        | 0.05          | 22013          |
| neat-csv (async/promise) | 9.69         | 0.05          | 20947          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 212460         | 98.27        | 0.00          |
| cisv (sync)        | 154323         | 71.38        | 0.01          |
| udsv (sync)        | 151442         | 70.05        | 0.01          |
| papaparse (sync)   | 60707          | 28.08        | 0.02          |
| csv-parse (sync)   | 40718          | 18.83        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 215118         | 99.50        | 0.00          |
| udsv (async/stream)      | 113115         | 52.32        | 0.01          |
| papaparse (async/stream) | 45538          | 21.06        | 0.02          |
| fast-csv (async/stream)  | 22387          | 10.35        | 0.04          |
| neat-csv (async/promise) | 20314          | 9.40         | 0.05          |


Cleaning up test files...
  Removed small.csv
  Removed medium.csv

Benchmark complete!

