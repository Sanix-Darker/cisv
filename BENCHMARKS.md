## BENCHMARK RESULTS
**DATE:** Thu Oct 16 23:44:48 UTC 2025

**COMMIT:** cb48d0d2b914208ac06852a28c3a2b6e5cf29194
---


Generating test CSV files...
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
  Progress: 0%

## CLI BENCHMARKS


### Testing with small.csv

File info: 0.09 MB, 1001 rows

#### Row counting test:

--- cisv ---
```

  Run 1: 0.00896168s
  Run 2: 0.00826764s
  Run 3: 0.00800776s

```
  Average Time: 0.0084 seconds
  Successful runs: 3/3
--- rust-csv ---
```

  Run 1: 0.00829887s
  Run 2: 0.00857353s
  Run 3: 0.00828123s

```
  Average Time: 0.0084 seconds
  Successful runs: 3/3
--- wc -l ---
```

  Run 1: 0.00830793s
  Run 2: 0.00838375s
  Run 3: 0.00828075s

```
  Average Time: 0.0083 seconds
  Successful runs: 3/3
--- csvkit ---
```

  Run 1: 0.285857s
  Run 2: 0.125828s
  Run 3: 0.122355s

```
  Average Time: 0.1780 seconds
  Successful runs: 3/3

=== Sorted Results: Row Counting - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        10.84 |       0.0083 |         120.48 |
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| cisv                 |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.51 |       0.1780 |           5.62 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        10.84 |       0.0083 |         120.48 |
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| cisv                 |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.51 |       0.1780 |           5.62 |

Column selection test (columns 0,2,3):

--- cisv ---
```

  Run 1: 0.00848436s
  Run 2: 0.00847578s
  Run 3: 0.00912094s

```
  Average Time: 0.0087 seconds
  Successful runs: 3/3
--- rust-csv ---
```

  Run 1: 0.00865126s
  Run 2: 0.00849271s
  Run 3: 0.00855732s

```
  Average Time: 0.0086 seconds
  Successful runs: 3/3
--- csvkit ---
```

  Run 1: 0.126121s
  Run 2: 0.122047s
  Run 3: 0.123368s

```
  Average Time: 0.1238 seconds
  Successful runs: 3/3

=== Sorted Results: Column Selection - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.47 |       0.0086 |         116.28 |
| cisv                 |        10.34 |       0.0087 |         114.94 |
| csvkit               |         0.73 |       0.1238 |           8.08 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.47 |       0.0086 |         116.28 |
| cisv                 |        10.34 |       0.0087 |         114.94 |
| csvkit               |         0.73 |       0.1238 |           8.08 |

### Testing with medium.csv

File info: 10.25 MB, 100001 rows

#### Row counting test:

--- cisv ---
```

  Run 1: 0.00946832s
  Run 2: 0.00966048s
  Run 3: 0.00965381s

```
  Average Time: 0.0096 seconds
  Successful runs: 3/3
--- rust-csv ---
```

  Run 1: 0.0254514s
  Run 2: 0.0259845s
  Run 3: 0.0250032s

```
  Average Time: 0.0255 seconds
  Successful runs: 3/3
--- wc -l ---
```

  Run 1: 0.00987244s
  Run 2: 0.00990939s
  Run 3: 0.00995445s

```
  Average Time: 0.0099 seconds
  Successful runs: 3/3
--- csvkit ---
```

  Run 1: 0.296501s
  Run 2: 0.29733s
  Run 3: 0.286818s

```
  Average Time: 0.2935 seconds
  Successful runs: 3/3

=== Sorted Results: Row Counting - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1067.71 |       0.0096 |         104.17 |
| wc -l                |      1035.35 |       0.0099 |         101.01 |
| rust-csv             |       401.96 |       0.0255 |          39.22 |
| csvkit               |        34.92 |       0.2935 |           3.41 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1067.71 |       0.0096 |         104.17 |
| wc -l                |      1035.35 |       0.0099 |         101.01 |
| rust-csv             |       401.96 |       0.0255 |          39.22 |
| csvkit               |        34.92 |       0.2935 |           3.41 |

Column selection test (columns 0,2,3):

--- cisv ---
```

  Run 1: 0.0513973s
  Run 2: 0.0514696s
  Run 3: 0.0516763s

```
  Average Time: 0.0515 seconds
  Successful runs: 3/3
--- rust-csv ---
```

  Run 1: 0.0400519s
  Run 2: 0.0394375s
  Run 3: 0.0398226s

```
  Average Time: 0.0398 seconds
  Successful runs: 3/3
--- csvkit ---
```

  Run 1: 0.363572s
  Run 2: 0.355073s
  Run 3: 0.355952s

```
  Average Time: 0.3582 seconds
  Successful runs: 3/3

=== Sorted Results: Column Selection - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       257.54 |       0.0398 |          25.13 |
| cisv                 |       199.03 |       0.0515 |          19.42 |
| csvkit               |        28.62 |       0.3582 |           2.79 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       257.54 |       0.0398 |          25.13 |
| cisv                 |       199.03 |       0.0515 |          19.42 |
| csvkit               |        28.62 |       0.3582 |           2.79 |

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
  cisv (sync) x 151,749 ops/sec ±2.27% (92 runs sampled)
    Speed: 70.19 MB/s | Avg Time: 0.01 ms | Ops/sec: 151749
    (cooling down...)

```
```
  csv-parse (sync) x 40,658 ops/sec ±2.18% (96 runs sampled)
    Speed: 18.81 MB/s | Avg Time: 0.02 ms | Ops/sec: 40658
    (cooling down...)

```
```
  papaparse (sync) x 60,287 ops/sec ±0.85% (94 runs sampled)
    Speed: 27.88 MB/s | Avg Time: 0.02 ms | Ops/sec: 60287
    (cooling down...)

```
```
  udsv (sync) x 149,076 ops/sec ±0.47% (94 runs sampled)
    Speed: 68.95 MB/s | Avg Time: 0.01 ms | Ops/sec: 149076
    (cooling down...)

```
```
  d3-dsv (sync) x 206,431 ops/sec ±0.95% (96 runs sampled)
    Speed: 95.48 MB/s | Avg Time: 0.00 ms | Ops/sec: 206431
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
```
  cisv (sync) x 226,848 ops/sec ±8.87% (70 runs sampled)
    Speed: 104.92 MB/s | Avg Time: 0.00 ms | Ops/sec: 226848
    (cooling down...)

```
```
  csv-parse (sync) x 42,462 ops/sec ±0.12% (97 runs sampled)
    Speed: 19.64 MB/s | Avg Time: 0.02 ms | Ops/sec: 42462
    (cooling down...)

```
```
  papaparse (sync) x 60,343 ops/sec ±0.83% (93 runs sampled)
    Speed: 27.91 MB/s | Avg Time: 0.02 ms | Ops/sec: 60343
    (cooling down...)

```
```
  udsv (sync) x 150,130 ops/sec ±0.50% (92 runs sampled)
    Speed: 69.44 MB/s | Avg Time: 0.01 ms | Ops/sec: 150130
    (cooling down...)

```
```
  d3-dsv (sync) x 211,003 ops/sec ±0.28% (96 runs sampled)
    Speed: 97.60 MB/s | Avg Time: 0.00 ms | Ops/sec: 211003
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
```
  cisv (async/stream) x 209,934 ops/sec ±0.60% (72 runs sampled)
    Speed: 97.10 MB/s | Avg Time: 0.00 ms | Ops/sec: 209934
    (cooling down...)

```
```
  papaparse (async/stream) x 44,552 ops/sec ±3.92% (83 runs sampled)
    Speed: 20.61 MB/s | Avg Time: 0.02 ms | Ops/sec: 44552
    (cooling down...)

```
```
  fast-csv (async/stream) x 21,451 ops/sec ±1.39% (83 runs sampled)
    Speed: 9.92 MB/s | Avg Time: 0.05 ms | Ops/sec: 21451
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,214 ops/sec ±2.66% (82 runs sampled)
    Speed: 9.35 MB/s | Avg Time: 0.05 ms | Ops/sec: 20214
    (cooling down...)

```
```
  udsv (async/stream) x 112,990 ops/sec ±0.29% (87 runs sampled)
    Speed: 52.26 MB/s | Avg Time: 0.01 ms | Ops/sec: 112990
    (cooling down...)

```

 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
```
  cisv (async/stream) x 58,119 ops/sec ±0.70% (86 runs sampled)
    Speed: 26.88 MB/s | Avg Time: 0.02 ms | Ops/sec: 58119
    (cooling down...)

```
```
  papaparse (async/stream) x 45,592 ops/sec ±5.65% (86 runs sampled)
    Speed: 21.09 MB/s | Avg Time: 0.02 ms | Ops/sec: 45592
    (cooling down...)

```
```
  fast-csv (async/stream) x 20,958 ops/sec ±0.22% (88 runs sampled)
    Speed: 9.69 MB/s | Avg Time: 0.05 ms | Ops/sec: 20958
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,695 ops/sec ±1.08% (88 runs sampled)
    Speed: 9.57 MB/s | Avg Time: 0.05 ms | Ops/sec: 20695
    (cooling down...)

```
```
  udsv (async/stream) x 112,230 ops/sec ±0.68% (90 runs sampled)
    Speed: 51.91 MB/s | Avg Time: 0.01 ms | Ops/sec: 112230
    (cooling down...)

```

 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 95.48        | 0.00          | 206431         |
| cisv (sync)        | 70.19        | 0.01          | 151749         |
| udsv (sync)        | 68.95        | 0.01          | 149076         |
| papaparse (sync)   | 27.88        | 0.02          | 60287          |
| csv-parse (sync)   | 18.81        | 0.02          | 40658          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 104.92       | 0.00          | 226848         |
| d3-dsv (sync)      | 97.60        | 0.00          | 211003         |
| udsv (sync)        | 69.44        | 0.01          | 150130         |
| papaparse (sync)   | 27.91        | 0.02          | 60343          |
| csv-parse (sync)   | 19.64        | 0.02          | 42462          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 97.10        | 0.00          | 209934         |
| udsv (async/stream)      | 52.26        | 0.01          | 112990         |
| papaparse (async/stream) | 20.61        | 0.02          | 44552          |
| fast-csv (async/stream)  | 9.92         | 0.05          | 21451          |
| neat-csv (async/promise) | 9.35         | 0.05          | 20214          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 51.91        | 0.01          | 112230         |
| cisv (async/stream)      | 26.88        | 0.02          | 58119          |
| papaparse (async/stream) | 21.09        | 0.02          | 45592          |
| fast-csv (async/stream)  | 9.69         | 0.05          | 20958          |
| neat-csv (async/promise) | 9.57         | 0.05          | 20695          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 206431         | 95.48        | 0.00          |
| cisv (sync)        | 151749         | 70.19        | 0.01          |
| udsv (sync)        | 149076         | 68.95        | 0.01          |
| papaparse (sync)   | 60287          | 27.88        | 0.02          |
| csv-parse (sync)   | 40658          | 18.81        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 209934         | 97.10        | 0.00          |
| udsv (async/stream)      | 112990         | 52.26        | 0.01          |
| papaparse (async/stream) | 44552          | 20.61        | 0.02          |
| fast-csv (async/stream)  | 21451          | 9.92         | 0.05          |
| neat-csv (async/promise) | 20214          | 9.35         | 0.05          |


Cleaning up test files...
  Removed small.csv
  Removed medium.csv

Benchmark complete!

