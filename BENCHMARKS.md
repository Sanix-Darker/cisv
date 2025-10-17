## BENCHMARK RESULTS

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
  Run 1: 0.00864291s
  Run 2: 0.0079689s
  Run 3: 0.0079627s
  Average Time: 0.0082 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.00858474s
  Run 2: 0.00835633s
  Run 3: 0.00861716s
  Average Time: 0.0085 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.00843644s
  Run 2: 0.00852251s
  Run 3: 0.00856733s
  Average Time: 0.0085 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.251271s
  Run 2: 0.127283s
  Run 3: 0.126482s
  Average Time: 0.1683 seconds
  Successful runs: 3/3

```

=== Sorted Results: Row Counting - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |        10.98 |       0.0082 |         121.95 |
| wc -l                |        10.59 |       0.0085 |         117.65 |
| rust-csv             |        10.59 |       0.0085 |         117.65 |
| csvkit               |         0.53 |       0.1683 |           5.94 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |        10.98 |       0.0082 |         121.95 |
| wc -l                |        10.59 |       0.0085 |         117.65 |
| rust-csv             |        10.59 |       0.0085 |         117.65 |
| csvkit               |         0.53 |       0.1683 |           5.94 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.0087111s
  Run 2: 0.00856948s
  Run 3: 0.00862646s
  Average Time: 0.0086 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.0087769s
  Run 2: 0.0087316s
  Run 3: 0.00887251s
  Average Time: 0.0088 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.132707s
  Run 2: 0.126181s
  Run 3: 0.123904s
  Average Time: 0.1276 seconds
  Successful runs: 3/3

```

=== Sorted Results: Column Selection - small.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |        10.47 |       0.0086 |         116.28 |
| rust-csv             |        10.23 |       0.0088 |         113.64 |
| csvkit               |         0.71 |       0.1276 |           7.84 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |        10.47 |       0.0086 |         116.28 |
| rust-csv             |        10.23 |       0.0088 |         113.64 |
| csvkit               |         0.71 |       0.1276 |           7.84 |

### Testing with medium.csv

File info: 10.25 MB, 100001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.0097394s
  Run 2: 0.00967288s
  Run 3: 0.00973654s
  Average Time: 0.0097 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.0263851s
  Run 2: 0.0265133s
  Run 3: 0.0256376s
  Average Time: 0.0262 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0101235s
  Run 2: 0.00998306s
  Run 3: 0.0100834s
  Average Time: 0.0101 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.293561s
  Run 2: 0.29493s
  Run 3: 0.295329s
  Average Time: 0.2946 seconds
  Successful runs: 3/3

```

=== Sorted Results: Row Counting - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1056.70 |       0.0097 |         103.09 |
| wc -l                |      1014.85 |       0.0101 |          99.01 |
| rust-csv             |       391.22 |       0.0262 |          38.17 |
| csvkit               |        34.79 |       0.2946 |           3.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      1056.70 |       0.0097 |         103.09 |
| wc -l                |      1014.85 |       0.0101 |          99.01 |
| rust-csv             |       391.22 |       0.0262 |          38.17 |
| csvkit               |        34.79 |       0.2946 |           3.39 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.0516188s
  Run 2: 0.0527086s
  Run 3: 0.05231s
  Average Time: 0.0522 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.0405717s
  Run 2: 0.0392122s
  Run 3: 0.0404291s
  Average Time: 0.0401 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 0.362321s
  Run 2: 0.36552s
  Run 3: 0.362114s
  Average Time: 0.3633 seconds
  Successful runs: 3/3

```

=== Sorted Results: Column Selection - medium.csv ===

Sorted by Speed (MB/s) - Fastest First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       255.61 |       0.0401 |          24.94 |
| cisv                 |       196.36 |       0.0522 |          19.16 |
| csvkit               |        28.21 |       0.3633 |           2.75 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       255.61 |       0.0401 |          24.94 |
| cisv                 |       196.36 |       0.0522 |          19.16 |
| csvkit               |        28.21 |       0.3633 |           2.75 |

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
  cisv (sync) x 154,311 ops/sec ±2.32% (90 runs sampled)
    Speed: 71.37 MB/s | Avg Time: 0.01 ms | Ops/sec: 154311
    (cooling down...)

```
```
  csv-parse (sync) x 39,764 ops/sec ±2.23% (91 runs sampled)
    Speed: 18.39 MB/s | Avg Time: 0.03 ms | Ops/sec: 39764
    (cooling down...)

```
```
  papaparse (sync) x 59,599 ops/sec ±1.41% (94 runs sampled)
    Speed: 27.57 MB/s | Avg Time: 0.02 ms | Ops/sec: 59599
    (cooling down...)

```
```
  udsv (sync) x 152,067 ops/sec ±0.33% (97 runs sampled)
    Speed: 70.34 MB/s | Avg Time: 0.01 ms | Ops/sec: 152067
    (cooling down...)

```
```
  d3-dsv (sync) x 210,084 ops/sec ±0.40% (96 runs sampled)
    Speed: 97.17 MB/s | Avg Time: 0.00 ms | Ops/sec: 210084
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
```
  cisv (sync) x 228,019 ops/sec ±8.43% (70 runs sampled)
    Speed: 105.47 MB/s | Avg Time: 0.00 ms | Ops/sec: 228019
    (cooling down...)

```
```
  csv-parse (sync) x 36,544 ops/sec ±21.09% (94 runs sampled)
    Speed: 16.90 MB/s | Avg Time: 0.03 ms | Ops/sec: 36544
    (cooling down...)

```
```
  papaparse (sync) x 59,711 ops/sec ±0.79% (92 runs sampled)
    Speed: 27.62 MB/s | Avg Time: 0.02 ms | Ops/sec: 59711
    (cooling down...)

```
```
  udsv (sync) x 152,061 ops/sec ±0.29% (92 runs sampled)
    Speed: 70.33 MB/s | Avg Time: 0.01 ms | Ops/sec: 152061
    (cooling down...)

```
```
  d3-dsv (sync) x 211,475 ops/sec ±0.24% (98 runs sampled)
    Speed: 97.81 MB/s | Avg Time: 0.00 ms | Ops/sec: 211475
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
```
  cisv (async/stream) x 209,037 ops/sec ±1.09% (68 runs sampled)
    Speed: 96.69 MB/s | Avg Time: 0.00 ms | Ops/sec: 209037
    (cooling down...)

```
```
  papaparse (async/stream) x 46,418 ops/sec ±2.62% (87 runs sampled)
    Speed: 21.47 MB/s | Avg Time: 0.02 ms | Ops/sec: 46418
    (cooling down...)

```
```
  fast-csv (async/stream) x 21,491 ops/sec ±1.23% (84 runs sampled)
    Speed: 9.94 MB/s | Avg Time: 0.05 ms | Ops/sec: 21491
    (cooling down...)

```
```
  neat-csv (async/promise) x 19,722 ops/sec ±2.03% (82 runs sampled)
    Speed: 9.12 MB/s | Avg Time: 0.05 ms | Ops/sec: 19722
    (cooling down...)

```
```
  udsv (async/stream) x 113,647 ops/sec ±1.72% (89 runs sampled)
    Speed: 52.57 MB/s | Avg Time: 0.01 ms | Ops/sec: 113647
    (cooling down...)

```

 Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
```
  cisv (async/stream) x 59,569 ops/sec ±0.16% (87 runs sampled)
    Speed: 27.55 MB/s | Avg Time: 0.02 ms | Ops/sec: 59569
    (cooling down...)

```
```
  papaparse (async/stream) x 48,598 ops/sec ±1.53% (85 runs sampled)
    Speed: 22.48 MB/s | Avg Time: 0.02 ms | Ops/sec: 48598
    (cooling down...)

```
```
  fast-csv (async/stream) x 21,456 ops/sec ±0.56% (87 runs sampled)
    Speed: 9.92 MB/s | Avg Time: 0.05 ms | Ops/sec: 21456
    (cooling down...)

```
```
  neat-csv (async/promise) x 20,584 ops/sec ±1.51% (88 runs sampled)
    Speed: 9.52 MB/s | Avg Time: 0.05 ms | Ops/sec: 20584
    (cooling down...)

```
```
  udsv (async/stream) x 114,703 ops/sec ±1.73% (90 runs sampled)
    Speed: 53.05 MB/s | Avg Time: 0.01 ms | Ops/sec: 114703
    (cooling down...)

```

 Fastest Async is udsv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results (sorted by speed - fastest first)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| d3-dsv (sync)      | 97.17        | 0.00          | 210084         |
| cisv (sync)        | 71.37        | 0.01          | 154311         |
| udsv (sync)        | 70.34        | 0.01          | 152067         |
| papaparse (sync)   | 27.57        | 0.02          | 59599          |
| csv-parse (sync)   | 18.39        | 0.03          | 39764          |

### Synchronous Results (with data access - sorted by speed)

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 105.47       | 0.00          | 228019         |
| d3-dsv (sync)      | 97.81        | 0.00          | 211475         |
| udsv (sync)        | 70.33        | 0.01          | 152061         |
| papaparse (sync)   | 27.62        | 0.02          | 59711          |
| csv-parse (sync)   | 16.90        | 0.03          | 36544          |


### Asynchronous Results (sorted by speed - fastest first)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 96.69        | 0.00          | 209037         |
| udsv (async/stream)      | 52.57        | 0.01          | 113647         |
| papaparse (async/stream) | 21.47        | 0.02          | 46418          |
| fast-csv (async/stream)  | 9.94         | 0.05          | 21491          |
| neat-csv (async/promise) | 9.12         | 0.05          | 19722          |


### Asynchronous Results (with data access - sorted by speed)

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| udsv (async/stream)      | 53.05        | 0.01          | 114703         |
| cisv (async/stream)      | 27.55        | 0.02          | 59569          |
| papaparse (async/stream) | 22.48        | 0.02          | 48598          |
| fast-csv (async/stream)  | 9.92         | 0.05          | 21456          |
| neat-csv (async/promise) | 9.52         | 0.05          | 20584          |


## Alternative Sorting: By Operations/sec

### Synchronous Results (sorted by operations/sec)

| Library            | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------|----------------|--------------|---------------|
| d3-dsv (sync)      | 210084         | 97.17        | 0.00          |
| cisv (sync)        | 154311         | 71.37        | 0.01          |
| udsv (sync)        | 152067         | 70.34        | 0.01          |
| papaparse (sync)   | 59599          | 27.57        | 0.02          |
| csv-parse (sync)   | 39764          | 18.39        | 0.03          |

### Asynchronous Results (sorted by operations/sec)

| Library                  | Operations/sec | Speed (MB/s) | Avg Time (ms) |
|--------------------------|----------------|--------------|---------------|
| cisv (async/stream)      | 209037         | 96.69        | 0.00          |
| udsv (async/stream)      | 113647         | 52.57        | 0.01          |
| papaparse (async/stream) | 46418          | 21.47        | 0.02          |
| fast-csv (async/stream)  | 21491          | 9.94         | 0.05          |
| neat-csv (async/promise) | 19722          | 9.12         | 0.05          |


Cleaning up test files...
  Removed small.csv
  Removed medium.csv

Benchmark complete!

