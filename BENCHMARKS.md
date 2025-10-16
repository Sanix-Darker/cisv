## BENCHMARK RESULTS

**DATE:** Thu Oct 16 23:30:16 UTC 2025
**COMMIT:** daa667f444f88c12ebcd197edb71de1918201ec3---


Generating test CSV files...
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
  Progress: 0%

=== CLI Benchmark ===


=== Testing with small.csv ===

File info: 0.09 MB, 1001 rows

Row counting test:

--- cisv ---
  Run 1: 0.0091114s
  Run 2: 0.00854135s
  Run 3: 0.00817227s
  Average Time: 0.0086 seconds
  Successful runs: 3/3
--- rust-csv ---
  Run 1: 0.00836062s
  Run 2: 0.00829864s
  Run 3: 0.00825715s
  Average Time: 0.0083 seconds
  Successful runs: 3/3
--- wc -l ---
  Run 1: 0.00819755s
  Run 2: 0.00824213s
  Run 3: 0.00809455s
  Average Time: 0.0082 seconds
  Successful runs: 3/3
--- csvkit ---
  Run 1: 0.200537s
  Run 2: 0.117658s
  Run 3: 0.115736s
  Average Time: 0.1446 seconds
  Successful runs: 3/3

=== Sorted Results: Row Counting - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        10.98 |       0.0082 |         121.95 |
| rust-csv             |        10.84 |       0.0083 |         120.48 |
| cisv                 |        10.47 |       0.0086 |         116.28 |
| csvkit               |         0.62 |       0.1446 |           6.92 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |        10.98 |       0.0082 |         121.95 |
| rust-csv             |        10.84 |       0.0083 |         120.48 |
| cisv                 |        10.47 |       0.0086 |         116.28 |
| csvkit               |         0.62 |       0.1446 |           6.92 |

Column selection test (columns 0,2,3):

--- cisv ---
  Run 1: 0.00834846s
  Run 2: 0.00837994s
  Run 3: 0.00845814s
  Average Time: 0.0084 seconds
  Successful runs: 3/3
--- rust-csv ---
  Run 1: 0.00842404s
  Run 2: 0.0082829s
  Run 3: 0.00848293s
  Average Time: 0.0084 seconds
  Successful runs: 3/3
--- csvkit ---
  Run 1: 0.119828s
  Run 2: 0.116623s
  Run 3: 0.117071s
  Average Time: 0.1178 seconds
  Successful runs: 3/3

=== Sorted Results: Column Selection - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| cisv                 |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.76 |       0.1178 |           8.49 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |        10.71 |       0.0084 |         119.05 |
| cisv                 |        10.71 |       0.0084 |         119.05 |
| csvkit               |         0.76 |       0.1178 |           8.49 |
=============================================================


=== Testing with medium.csv ===

File info: 10.25 MB, 100001 rows

Row counting test:

--- cisv ---
  Run 1: 0.00917554s
  Run 2: 0.0089345s
  Run 3: 0.00896883s
  Average Time: 0.0090 seconds
  Successful runs: 3/3
--- rust-csv ---
  Run 1: 0.0259817s
  Run 2: 0.0260067s
  Run 3: 0.0261376s
  Average Time: 0.0260 seconds
  Successful runs: 3/3
--- wc -l ---
  Run 1: 0.0099256s
  Run 2: 0.00969744s
  Run 3: 0.00956416s
  Average Time: 0.0097 seconds
  Successful runs: 3/3
--- csvkit ---
  Run 1: 0.287847s
  Run 2: 0.285748s
  Run 3: 0.293718s
  Average Time: 0.2891 seconds
  Successful runs: 3/3

=== Sorted Results: Row Counting - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1138.89 |       0.0090 |         111.11 |
| wc -l                |      1056.70 |       0.0097 |         103.09 |
| rust-csv             |       394.23 |       0.0260 |          38.46 |
| csvkit               |        35.45 |       0.2891 |           3.46 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1138.89 |       0.0090 |         111.11 |
| wc -l                |      1056.70 |       0.0097 |         103.09 |
| rust-csv             |       394.23 |       0.0260 |          38.46 |
| csvkit               |        35.45 |       0.2891 |           3.46 |

Column selection test (columns 0,2,3):

--- cisv ---
  Run 1: 0.051682s
  Run 2: 0.051605s
  Run 3: 0.052002s
  Average Time: 0.0518 seconds
  Successful runs: 3/3
--- rust-csv ---
  Run 1: 0.03929s
  Run 2: 0.0394299s
  Run 3: 0.0394647s
  Average Time: 0.0394 seconds
  Successful runs: 3/3
--- csvkit ---
  Run 1: 0.358227s
  Run 2: 0.359088s
  Run 3: 0.354616s
  Average Time: 0.3573 seconds
  Successful runs: 3/3

=== Sorted Results: Column Selection - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       260.15 |       0.0394 |          25.38 |
| cisv                 |       197.88 |       0.0518 |          19.31 |
| csvkit               |        28.69 |       0.3573 |           2.80 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       260.15 |       0.0394 |          25.38 |
| cisv                 |       197.88 |       0.0518 |          19.31 |
| csvkit               |        28.69 |       0.3573 |           2.80 |
=============================================================


=== NPM Benchmark ===


> cisv@0.0.7 benchmark-js
> node benchmark/benchmark.js

 Using benchmark file: /home/runner/work/cisv/cisv/fixtures/data.csv
Starting benchmark with file: /home/runner/work/cisv/cisv/fixtures/data.csv
All tests will retrieve row index: 4

File size: 0.00 MB
Sample of target row: [ '4', 'Dana White', 'dana.white@email.com', 'Chicago' ]


--- Running: Sync (Parse only) Benchmarks ---
  cisv (sync) x 154,576 ops/sec ±2.09% (89 runs sampled)
    Speed: 71.50 MB/s | Avg Time: 0.01 ms | Ops/sec: 154576
    (cooling down...)

  csv-parse (sync) x 42,007 ops/sec ±0.57% (95 runs sampled)
    Speed: 19.43 MB/s | Avg Time: 0.02 ms | Ops/sec: 42007
    (cooling down...)

  papaparse (sync) x 60,139 ops/sec ±1.43% (97 runs sampled)
    Speed: 27.82 MB/s | Avg Time: 0.02 ms | Ops/sec: 60139
    (cooling down...)

  udsv (sync) x 151,823 ops/sec ±0.47% (95 runs sampled)
    Speed: 70.22 MB/s | Avg Time: 0.01 ms | Ops/sec: 151823
    (cooling down...)

  d3-dsv (sync) x 209,495 ops/sec ±1.36% (94 runs sampled)
    Speed: 96.90 MB/s | Avg Time: 0.00 ms | Ops/sec: 209495
    (cooling down...)


 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
  cisv (sync) x 229,617 ops/sec ±8.85% (70 runs sampled)
    Speed: 106.21 MB/s | Avg Time: 0.00 ms | Ops/sec: 229617
    (cooling down...)

  csv-parse (sync) x 37,950 ops/sec ±19.34% (96 runs sampled)
    Speed: 17.55 MB/s | Avg Time: 0.03 ms | Ops/sec: 37950
    (cooling down...)

  papaparse (sync) x 60,801 ops/sec ±0.79% (94 runs sampled)
    Speed: 28.12 MB/s | Avg Time: 0.02 ms | Ops/sec: 60801
    (cooling down...)

  udsv (sync) x 152,921 ops/sec ±0.32% (97 runs sampled)
    Speed: 70.73 MB/s | Avg Time: 0.01 ms | Ops/sec: 152921
    (cooling down...)

  d3-dsv (sync) x 208,820 ops/sec ±0.68% (94 runs sampled)
    Speed: 96.59 MB/s | Avg Time: 0.00 ms | Ops/sec: 208820
    (cooling down...)


 Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
  cisv (async/stream) x 212,798 ops/sec ±0.44% (72 runs sampled)
    Speed: 98.43 MB/s | Avg Time: 0.00 ms | Ops/sec: 212798
    (cooling down...)

  papaparse (async/stream) x 45,632 ops/sec ±3.61% (80 runs sampled)
    Speed: 21.11 MB/s | Avg Time: 0.02 ms | Ops/sec: 45632
    (cooling down...)

  fast-csv (async/stream) x 22,020 ops/sec ±1.07% (89 runs sampled)
    Speed: 10.19 MB/s | Avg Time: 0.05 ms | Ops/sec: 22020
    (cooling down...)

  neat-csv (async/promise) x 20,288 ops/sec ±1.80% (87 runs sampled)
    Speed: 9.38 MB/s | Avg Time: 0.05 ms | Ops/sec: 20288
    (cooling down...)

  udsv (async/stream) x 114,056 ops/sec ±0.34% (88 runs sampled)
    Speed: 52.75 MB/s | Avg Time: 0.01 ms | Ops/sec: 114056
    (cooling down...)


 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
  cisv (async/stream) x 59,598 ops/sec ±0.22% (87 runs sampled)
    Speed: 27.57 MB/s | Avg Time: 0.02 ms | Ops/sec: 59598
    (cooling down...)

  papaparse (async/stream) x 47,401 ops/sec ±2.23% (85 runs sampled)
    Speed: 21.92 MB/s | Avg Time: 0.02 ms | Ops/sec: 47401
    (cooling down...)

  fast-csv (async/stream) x 21,143 ops/sec ±1.24% (84 runs sampled)
    Speed: 9.78 MB/s | Avg Time: 0.05 ms | Ops/sec: 21143
    (cooling down...)

  neat-csv (async/promise) x 20,652 ops/sec ±1.22% (84 runs sampled)
    Speed: 9.55 MB/s | Avg Time: 0.05 ms | Ops/sec: 20652
    (cooling down...)

  udsv (async/stream) x 115,143 ops/sec ±0.37% (88 runs sampled)
    Speed: 53.26 MB/s | Avg Time: 0.01 ms | Ops/sec: 115143
    (cooling down...)


 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 96.90        | 0.00          | 209495         |
| cisv (sync)        | 71.50        | 0.01          | 154576         |
| udsv (sync)        | 70.22        | 0.01          | 151823         |
| papaparse (sync)   | 27.82        | 0.02          | 60139          |
| csv-parse (sync)   | 19.43        | 0.02          | 42007          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 106.21       | 0.00          | 229617         |
| d3-dsv (sync)      | 96.59        | 0.00          | 208820         |
| udsv (sync)        | 70.73        | 0.01          | 152921         |
| papaparse (sync)   | 28.12        | 0.02          | 60801          |
| csv-parse (sync)   | 17.55        | 0.03          | 37950          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 98.43        | 0.00          | 212798         |
| udsv (async/stream)      | 52.75        | 0.01          | 114056         |
| papaparse (async/stream) | 21.11        | 0.02          | 45632          |
| fast-csv (async/stream)  | 10.19        | 0.05          | 22020          |
| neat-csv (async/promise) | 9.38         | 0.05          | 20288          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 53.26        | 0.01          | 115143         |
| cisv (async/stream)      | 27.57        | 0.02          | 59598          |
| papaparse (async/stream) | 21.92        | 0.02          | 47401          |
| fast-csv (async/stream)  | 9.78         | 0.05          | 21143          |
| neat-csv (async/promise) | 9.55         | 0.05          | 20652          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 209495         | 96.90        | 0.00          |
| cisv (sync)        | 154576         | 71.50        | 0.01          |
| udsv (sync)        | 151823         | 70.22        | 0.01          |
| papaparse (sync)   | 60139          | 27.82        | 0.02          |
| csv-parse (sync)   | 42007          | 19.43        | 0.02          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 212798         | 98.43        | 0.00          |
| udsv (async/stream)      | 114056         | 52.75        | 0.01          |
| papaparse (async/stream) | 45632          | 21.11        | 0.02          |
| fast-csv (async/stream)  | 22020          | 10.19        | 0.05          |
| neat-csv (async/promise) | 20288          | 9.38         | 0.05          |


Cleaning up test files...
  Removed small.csv
  Removed medium.csv

Benchmark complete!

