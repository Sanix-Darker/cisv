## BENCHMARK RESULTS
DATE: Thu Oct 16 23:02:54 UTC 2025

[1;33mGenerating test CSV files...[0m
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
  Progress: 0%
[0;34m
=== CLI Benchmark ===
[0m
[0;32m
=== Testing with small.csv ===
[0m
[0;34mFile info: 0.09 MB, 1001 rows
[0m
[1;33mRow counting test:
[0m
[0;34m--- cisv ---[0m
  Run 1: 0.00883007s
  Run 2: 0.00777674s
  Run 3: 0.00769114s
  Average Time: 0.0081 seconds
  Successful runs: 3/3
[0;34m--- rust-csv ---[0m
  Run 1: 0.00809908s
  Run 2: 0.00798678s
  Run 3: 0.00822067s
  Average Time: 0.0081 seconds
  Successful runs: 3/3
[0;34m--- wc -l ---[0m
  Run 1: 0.0079987s
  Run 2: 0.00799894s
  Run 3: 0.0079987s
  Average Time: 0.0080 seconds
  Successful runs: 3/3
[0;34m--- csvkit ---[0m
  Run 1: 0.287767s
  Run 2: 0.113357s
  Run 3: 0.113367s
  Average Time: 0.1715 seconds
  Successful runs: 3/3
[0;32m
=== Sorted Results: Row Counting - small.csv ===[0m
[1;33m
Sorted by Speed (MB/s) - Fastest First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
wc -l                |        11.25 |       0.0080 |         125.00
rust-csv             |        11.11 |       0.0081 |         123.46
cisv                 |        11.11 |       0.0081 |         123.46
csvkit               |         0.52 |       0.1715 |           5.83
[1;33m
Sorted by Operations/sec - Most Operations First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
wc -l                |        11.25 |       0.0080 |         125.00
rust-csv             |        11.11 |       0.0081 |         123.46
cisv                 |        11.11 |       0.0081 |         123.46
csvkit               |         0.52 |       0.1715 |           5.83
[1;33m
Column selection test (columns 0,2,3):
[0m
[0;34m--- cisv ---[0m
  Run 1: 0.00824165s
  Run 2: 0.00821924s
  Run 3: 0.00825548s
  Average Time: 0.0082 seconds
  Successful runs: 3/3
[0;34m--- rust-csv ---[0m
  Run 1: 0.00832725s
  Run 2: 0.0083313s
  Run 3: 0.00823426s
  Average Time: 0.0083 seconds
  Successful runs: 3/3
[0;34m--- csvkit ---[0m
  Run 1: 0.115689s
  Run 2: 0.114551s
  Run 3: 0.115879s
  Average Time: 0.1154 seconds
  Successful runs: 3/3
[0;32m
=== Sorted Results: Column Selection - small.csv ===[0m
[1;33m
Sorted by Speed (MB/s) - Fastest First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
cisv                 |        10.98 |       0.0082 |         121.95
rust-csv             |        10.84 |       0.0083 |         120.48
csvkit               |         0.78 |       0.1154 |           8.67
[1;33m
Sorted by Operations/sec - Most Operations First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
cisv                 |        10.98 |       0.0082 |         121.95
rust-csv             |        10.84 |       0.0083 |         120.48
csvkit               |         0.78 |       0.1154 |           8.67
[0;34m=============================================================
[0m
[0;32m
=== Testing with medium.csv ===
[0m
[0;34mFile info: 10.25 MB, 100001 rows
[0m
[1;33mRow counting test:
[0m
[0;34m--- cisv ---[0m
  Run 1: 0.00917506s
  Run 2: 0.00889015s
  Run 3: 0.00892591s
  Average Time: 0.0090 seconds
  Successful runs: 3/3
[0;34m--- rust-csv ---[0m
  Run 1: 0.0250483s
  Run 2: 0.0249741s
  Run 3: 0.0252364s
  Average Time: 0.0251 seconds
  Successful runs: 3/3
[0;34m--- wc -l ---[0m
  Run 1: 0.00911283s
  Run 2: 0.00919056s
  Run 3: 0.00926495s
  Average Time: 0.0092 seconds
  Successful runs: 3/3
[0;34m--- csvkit ---[0m
  Run 1: 0.283649s
  Run 2: 0.282694s
  Run 3: 0.281788s
  Average Time: 0.2827 seconds
  Successful runs: 3/3
[0;32m
=== Sorted Results: Row Counting - medium.csv ===[0m
[1;33m
Sorted by Speed (MB/s) - Fastest First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
cisv                 |      1138.89 |       0.0090 |         111.11
wc -l                |      1114.13 |       0.0092 |         108.70
rust-csv             |       408.37 |       0.0251 |          39.84
csvkit               |        36.26 |       0.2827 |           3.54
[1;33m
Sorted by Operations/sec - Most Operations First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
cisv                 |      1138.89 |       0.0090 |         111.11
wc -l                |      1114.13 |       0.0092 |         108.70
rust-csv             |       408.37 |       0.0251 |          39.84
csvkit               |        36.26 |       0.2827 |           3.54
[1;33m
Column selection test (columns 0,2,3):
[0m
[0;34m--- cisv ---[0m
  Run 1: 0.0519896s
  Run 2: 0.0510161s
  Run 3: 0.0506413s
  Average Time: 0.0512 seconds
  Successful runs: 3/3
[0;34m--- rust-csv ---[0m
  Run 1: 0.0395322s
  Run 2: 0.03934s
  Run 3: 0.0392616s
  Average Time: 0.0394 seconds
  Successful runs: 3/3
[0;34m--- csvkit ---[0m
  Run 1: 0.357471s
  Run 2: 0.350667s
  Run 3: 0.353868s
  Average Time: 0.3540 seconds
  Successful runs: 3/3
[0;32m
=== Sorted Results: Column Selection - medium.csv ===[0m
[1;33m
Sorted by Speed (MB/s) - Fastest First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
rust-csv             |       260.15 |       0.0394 |          25.38
cisv                 |       200.20 |       0.0512 |          19.53
csvkit               |        28.95 |       0.3540 |           2.82
[1;33m
Sorted by Operations/sec - Most Operations First:[0m
Library              | Speed (MB/s) | Avg Time (s) | Operations/sec
---------------------+--------------+--------------+---------------
rust-csv             |       260.15 |       0.0394 |          25.38
cisv                 |       200.20 |       0.0512 |          19.53
csvkit               |        28.95 |       0.3540 |           2.82
[0;34m=============================================================
[0m
[0;34m
=== NPM Benchmark ===
[0m

> cisv@0.0.7 benchmark-js
> node benchmark/benchmark.js

>>> Using benchmark file: /home/runner/work/cisv/cisv/fixtures/data.csv
Starting benchmark with file: /home/runner/work/cisv/cisv/fixtures/data.csv
All tests will retrieve row index: 4

File size: 0.00 MB
Sample of target row: [ '4', 'Dana White', 'dana.white@email.com', 'Chicago' ]


--- Running: Sync (Parse only) Benchmarks ---
  cisv (sync) x 156,264 ops/sec ±1.92% (90 runs sampled)
    Speed: 72.28 MB/s | Avg Time: 0.01 ms | Ops/sec: 156264
    (cooling down...)

  csv-parse (sync) x 40,954 ops/sec ±1.92% (93 runs sampled)
    Speed: 18.94 MB/s | Avg Time: 0.02 ms | Ops/sec: 40954
    (cooling down...)

  papaparse (sync) x 61,453 ops/sec ±0.98% (98 runs sampled)
    Speed: 28.42 MB/s | Avg Time: 0.02 ms | Ops/sec: 61453
    (cooling down...)

  udsv (sync) x 150,556 ops/sec ±0.50% (96 runs sampled)
    Speed: 69.64 MB/s | Avg Time: 0.01 ms | Ops/sec: 150556
    (cooling down...)

  d3-dsv (sync) x 203,366 ops/sec ±0.35% (98 runs sampled)
    Speed: 94.06 MB/s | Avg Time: 0.00 ms | Ops/sec: 203366
    (cooling down...)


>>>>> Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
  cisv (sync) x 230,885 ops/sec ±8.64% (73 runs sampled)
    Speed: 106.79 MB/s | Avg Time: 0.00 ms | Ops/sec: 230885
    (cooling down...)

  csv-parse (sync) x 42,485 ops/sec ±0.12% (99 runs sampled)
    Speed: 19.65 MB/s | Avg Time: 0.02 ms | Ops/sec: 42485
    (cooling down...)

  papaparse (sync) x 61,756 ops/sec ±0.72% (98 runs sampled)
    Speed: 28.56 MB/s | Avg Time: 0.02 ms | Ops/sec: 61756
    (cooling down...)

  udsv (sync) x 154,571 ops/sec ±0.39% (98 runs sampled)
    Speed: 71.49 MB/s | Avg Time: 0.01 ms | Ops/sec: 154571
    (cooling down...)

  d3-dsv (sync) x 210,260 ops/sec ±0.35% (97 runs sampled)
    Speed: 97.25 MB/s | Avg Time: 0.00 ms | Ops/sec: 210260
    (cooling down...)


>>>>> Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
  cisv (async/stream) x 211,982 ops/sec ±0.83% (75 runs sampled)
    Speed: 98.05 MB/s | Avg Time: 0.00 ms | Ops/sec: 211982
    (cooling down...)

  papaparse (async/stream) x 45,740 ops/sec ±3.47% (81 runs sampled)
    Speed: 21.16 MB/s | Avg Time: 0.02 ms | Ops/sec: 45740
    (cooling down...)

  fast-csv (async/stream) x 22,441 ops/sec ±1.11% (88 runs sampled)
    Speed: 10.38 MB/s | Avg Time: 0.04 ms | Ops/sec: 22441
    (cooling down...)

  neat-csv (async/promise) x 20,380 ops/sec ±2.50% (80 runs sampled)
    Speed: 9.43 MB/s | Avg Time: 0.05 ms | Ops/sec: 20380
    (cooling down...)

  udsv (async/stream) x 112,795 ops/sec ±0.41% (88 runs sampled)
    Speed: 52.17 MB/s | Avg Time: 0.01 ms | Ops/sec: 112795
    (cooling down...)


>>>>> Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
  cisv (async/stream) x 59,709 ops/sec ±0.35% (85 runs sampled)
    Speed: 27.62 MB/s | Avg Time: 0.02 ms | Ops/sec: 59709
    (cooling down...)

  papaparse (async/stream) x 48,024 ops/sec ±1.97% (88 runs sampled)
    Speed: 22.21 MB/s | Avg Time: 0.02 ms | Ops/sec: 48024
    (cooling down...)

  fast-csv (async/stream) x 21,968 ops/sec ±0.37% (87 runs sampled)
    Speed: 10.16 MB/s | Avg Time: 0.05 ms | Ops/sec: 21968
    (cooling down...)

  neat-csv (async/promise) x 20,919 ops/sec ±1.11% (84 runs sampled)
    Speed: 9.68 MB/s | Avg Time: 0.05 ms | Ops/sec: 20919
    (cooling down...)

  udsv (async/stream) x 115,014 ops/sec ±0.36% (89 runs sampled)
    Speed: 53.20 MB/s | Avg Time: 0.01 ms | Ops/sec: 115014
    (cooling down...)


>>>>> Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 94.06        | 0.00          | 203366         |
| cisv (sync)        | 72.28        | 0.01          | 156264         |
| udsv (sync)        | 69.64        | 0.01          | 150556         |
| papaparse (sync)   | 28.42        | 0.02          | 61453          |
| csv-parse (sync)   | 18.94        | 0.02          | 40954          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 106.79       | 0.00          | 230885         |
| d3-dsv (sync)      | 97.25        | 0.00          | 210260         |
| udsv (sync)        | 71.49        | 0.01          | 154571         |
| papaparse (sync)   | 28.56        | 0.02          | 61756          |
| csv-parse (sync)   | 19.65        | 0.02          | 42485          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 98.05        | 0.00          | 211982         |
| udsv (async/stream)      | 52.17        | 0.01          | 112795         |
| papaparse (async/stream) | 21.16        | 0.02          | 45740          |
| fast-csv (async/stream)  | 10.38        | 0.04          | 22441          |
| neat-csv (async/promise) | 9.43         | 0.05          | 20380          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 53.20        | 0.01          | 115014         |
| cisv (async/stream)      | 27.62        | 0.02          | 59709          |
| papaparse (async/stream) | 22.21        | 0.02          | 48024          |
| fast-csv (async/stream)  | 10.16        | 0.05          | 21968          |
| neat-csv (async/promise) | 9.68         | 0.05          | 20919          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 203366         | 94.06        | 0.00          |
| cisv (sync)        | 156264         | 72.28        | 0.01          |
| udsv (sync)        | 150556         | 69.64        | 0.01          |
| papaparse (sync)   | 61453          | 28.42        | 0.02          |
| csv-parse (sync)   | 40954          | 18.94        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 211982         | 98.05        | 0.00          |
| udsv (async/stream)      | 112795         | 52.17        | 0.01          |
| papaparse (async/stream) | 45740          | 21.16        | 0.02          |
| fast-csv (async/stream)  | 22441          | 10.38        | 0.04          |
| neat-csv (async/promise) | 20380          | 9.43         | 0.05          |

[1;33m
Cleaning up test files...[0m
  Removed small.csv
  Removed medium.csv
[0;32m
Benchmark complete![0m
