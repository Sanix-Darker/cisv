## BEFORE

$ bash ./benchmark_cli_reader.sh
=== CSV CLI Tools Benchmark ===

Generating test files...
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
Creating large.csv (1M rows)...
Creating xlarge.csv (10M rows)...

Test files created:
-rw-rw-r-- 1 dk dk  98M Aug  7 23:47 large.csv
-rw-rw-r-- 1 dk dk 9.2M Aug  7 23:47 medium.csv
-rw-rw-r-- 1 dk dk  84K Aug  7 23:47 small.csv
-rw-rw-r-- 1 dk dk   36 Jul 29 23:43 test.csv
-rw-rw-r-- 1 dk dk 1.1G Aug  7 23:48 xlarge.csv

=== Testing with small.csv ===

Row counting test:

--- cisv ---
Time: .003454255 seconds
Memory: 3444 KB
--- wc -l (baseline) ---
Time: .003698894 seconds
Memory: 3276 KB
--- rust-csv (Rust library) ---
Time: .005211807 seconds
Memory: 3268 KB
--- xsv (Rust CLI) ---
Time: .019139847 seconds
Memory: 5744 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: .389906267 seconds
Memory: 25604 KB
--- miller ---
Time: .005465541 seconds
Memory: 3404 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: .005140956 seconds
Memory: 3348 KB
--- rust-csv ---
Time: .004899935 seconds
Memory: 3160 KB
--- xsv ---
Time: .015615774 seconds
Memory: 5732 KB
qsv: Not installed

--- csvkit ---
Time: .370897805 seconds
Memory: 25080 KB
--- miller ---
Time: .008411697 seconds
Memory: 3308 KB
==================================================

=== Testing with medium.csv ===

Row counting test:

--- cisv ---
Time: .012855018 seconds
Memory: 8696 KB
--- wc -l (baseline) ---
Time: .010436686 seconds
Memory: 3356 KB
--- rust-csv (Rust library) ---
Time: .076268404 seconds
Memory: 3312 KB
--- xsv (Rust CLI) ---
Time: .063432975 seconds
Memory: 5780 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: 1.002472719 seconds
Memory: 80992 KB
--- miller ---
Time: .003949264 seconds
Memory: 3484 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: .194812322 seconds
Memory: 8456 KB
--- rust-csv ---
Time: .107812861 seconds
Memory: 3320 KB
--- xsv ---
Time: .084669622 seconds
Memory: 5704 KB
qsv: Not installed

--- csvkit ---
Time: 1.388574721 seconds
Memory: 24992 KB
--- miller ---
Time: .401692577 seconds
Memory: 11896 KB
==================================================

=== Testing with large.csv ===

Row counting test:

--- cisv ---
Time: .113134726 seconds
Memory: 97344 KB
--- wc -l (baseline) ---
Time: .081953593 seconds
Memory: 3328 KB
--- rust-csv (Rust library) ---
Time: .867276385 seconds
Memory: 3452 KB
--- xsv (Rust CLI) ---
Time: .563000612 seconds
Memory: 5896 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: 9.668958975 seconds
Memory: 582168 KB
--- miller ---
Time: .004417748 seconds
Memory: 3420 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: 1.856142346 seconds
Memory: 101416 KB
--- rust-csv ---
Time: 1.064498714 seconds
Memory: 3308 KB
--- xsv ---
Time: .745167991 seconds
Memory: 5776 KB
qsv: Not installed

--- csvkit ---
Time: 11.481508682 seconds
Memory: 24976 KB
--- miller ---
Time: 3.587480127 seconds
Memory: 102560 KB
==================================================

=== Testing with xlarge.csv (limited tools) ===

Row counting test:

--- cisv ---
Time: 1.027678668 seconds
Memory: 1056188 KB
--- wc -l (baseline) ---
Time: .735034280 seconds
Memory: 3244 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: 19.634665027 seconds
Memory: 1060368 KB
==================================================

=== CISV Benchmark Mode ===

small.csv:
Benchmarking file: small.csv
File size: 0.08 MB

Run 1: 0.06 ms, 1001 rows
Run 2: 0.04 ms, 1001 rows
Run 3: 0.03 ms, 1001 rows
Run 4: 0.03 ms, 1001 rows
Run 5: 0.03 ms, 1001 rows

medium.csv:
Benchmarking file: medium.csv
File size: 9.20 MB

Run 1: 4.10 ms, 100001 rows
Run 2: 3.89 ms, 100001 rows
Run 3: 4.05 ms, 100001 rows
Run 4: 3.94 ms, 100001 rows
Run 5: 3.15 ms, 100001 rows

large.csv:
Benchmarking file: large.csv
File size: 97.70 MB

Run 1: 33.20 ms, 1000001 rows
Run 2: 32.19 ms, 1000001 rows
Run 3: 31.98 ms, 1000001 rows
Run 4: 31.93 ms, 1000001 rows
Run 5: 31.94 ms, 1000001 rows

xlarge.csv:
Benchmarking file: xlarge.csv
File size: 1034.18 MB

Run 1: 329.84 ms, 10000001 rows
Run 2: 323.77 ms, 10000001 rows
Run 3: 359.50 ms, 10000001 rows
Run 4: 375.55 ms, 10000001 rows
Run 5: 324.35 ms, 10000001 rows

Cleaning up test files...
Benchmark complete!

## AFTER

$ bash ./benchmark_cli_reader.sh
=== CSV CLI Tools Benchmark ===                                                                                                                       Generating test files...
Creating small.csv (1K rows)...
Creating medium.csv (100K rows)...
Creating large.csv (1M rows)...
Creating xlarge.csv (10M rows)...

Test files created:
-rw-rw-r-- 1 dk dk  98M Aug  8 00:01 large.csv
-rw-rw-r-- 1 dk dk 9.2M Aug  8 00:00 medium.csv
-rw-rw-r-- 1 dk dk  84K Aug  8 00:00 small.csv
-rw-rw-r-- 1 dk dk   36 Jul 29 23:43 test.csv
-rw-rw-r-- 1 dk dk 1.1G Aug  8 00:01 xlarge.csv

=== Testing with small.csv ===

Row counting test:

--- cisv ---
Time: .004275249 seconds
Memory: 3388 KB
--- wc -l (baseline) ---
Time: .003821359 seconds
Memory: 3384 KB
--- rust-csv (Rust library) ---
Time: .005872746 seconds
Memory: 3252 KB
--- xsv (Rust CLI) ---
Time: .018956288 seconds
Memory: 5740 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: .324120841 seconds
Memory: 25556 KB
--- miller ---
Time: .003103605 seconds
Memory: 3280 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: .005032685 seconds
Memory: 3280 KB
--- rust-csv ---
Time: .004845613 seconds
Memory: 3332 KB
--- xsv ---
Time: .014999463 seconds
Memory: 5888 KB
qsv: Not installed

--- csvkit ---
Time: .327290599 seconds
Memory: 24888 KB
--- miller ---
Time: .006923448 seconds
Memory: 3356 KB
==================================================

=== Testing with medium.csv ===

Row counting test:

--- cisv ---
Time: .008108207 seconds
Memory: 8656 KB
--- wc -l (baseline) ---
Time: .010016111 seconds
Memory: 3432 KB
--- rust-csv (Rust library) ---
Time: .074509915 seconds
Memory: 3408 KB
--- xsv (Rust CLI) ---
Time: .053966146 seconds
Memory: 5704 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: .913265702 seconds
Memory: 81048 KB
--- miller ---
Time: .003564636 seconds
Memory: 3376 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: .135671992 seconds
Memory: 22716 KB
--- rust-csv ---
Time: .093014675 seconds
Memory: 3344 KB
--- xsv ---
Time: .073360775 seconds
Memory: 5720 KB
qsv: Not installed

--- csvkit ---
Time: 1.184642626 seconds
Memory: 24916 KB
--- miller ---
Time: .312788023 seconds
Memory: 11888 KB
==================================================

=== Testing with large.csv ===

Row counting test:

--- cisv ---
Time: .055322179 seconds
Memory: 97356 KB
--- wc -l (baseline) ---
Time: .083222782 seconds
Memory: 3340 KB
--- rust-csv (Rust library) ---
Time: .664236076 seconds
Memory: 3424 KB
--- xsv (Rust CLI) ---
Time: .441226534 seconds
Memory: 5648 KB
qsv (faster xsv fork): Not installed

--- csvkit (Python) ---
Time: 9.787511258 seconds
Memory: 582200 KB
--- miller ---
Time: .003772011 seconds
Memory: 3152 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: 1.370785745 seconds
Memory: 218840 KB
--- rust-csv ---
Time: .932174528 seconds
Memory: 3272 KB
--- xsv ---
Time: .654072621 seconds
Memory: 5880 KB
qsv: Not installed

--- csvkit ---
Time: 9.656998755 seconds
Memory: 25020 KB
--- miller ---
Time: 3.237759447 seconds
Memory: 102540 KB
==================================================

=== Testing with xlarge.csv (limited tools) ===

Row counting test:

--- cisv ---
Time: .590418509 seconds
Memory: 1056268 KB
--- wc -l (baseline) ---
Time: .783242068 seconds
Memory: 3272 KB
Column selection test (columns 0,2,3):

--- cisv ---
Time: 14.384985245 seconds
Memory: 2372932 KB
==================================================

=== CISV Benchmark Mode ===

small.csv:
Benchmarking file: small.csv
File size: 0.08 MB

Run 1: 0.02 ms, 1001 rows
Run 2: 0.01 ms, 1001 rows
Run 3: 0.01 ms, 1001 rows
Run 4: 0.01 ms, 1001 rows
Run 5: 0.01 ms, 1001 rows

medium.csv:
Benchmarking file: medium.csv
File size: 9.20 MB

Run 1: 1.64 ms, 100001 rows
Run 2: 1.24 ms, 100001 rows
Run 3: 1.13 ms, 100001 rows
Run 4: 1.22 ms, 100001 rows
Run 5: 2.00 ms, 100001 rows

large.csv:
Benchmarking file: large.csv
File size: 97.70 MB

Run 1: 19.82 ms, 1000001 rows
Run 2: 19.06 ms, 1000001 rows
Run 3: 19.29 ms, 1000001 rows
Run 4: 19.49 ms, 1000001 rows
Run 5: 19.41 ms, 1000001 rows

xlarge.csv:
Benchmarking file: xlarge.csv
File size: 1034.18 MB

Run 1: 192.34 ms, 10000001 rows
Run 2: 193.72 ms, 10000001 rows
Run 3: 195.66 ms, 10000001 rows
Run 4: 196.29 ms, 10000001 rows
Run 5: 203.84 ms, 10000001 rows

Cleaning up test files...
Benchmark complete!


=====================================================================================
from npm :
=====================================================================================

## BEFORE

$ npm run benchmark ./fixtures/data.csv

> cisv@0.0.1 benchmark
> node benchmark/benchmark.js ./fixtures/data.csv

Starting benchmark with file: ./fixtures/data.csv
All tests will retrieve row index: 857010

File size: 0.00 MB
Warning: File has fewer than 857011 rows. Tests will run but may not retrieve data.


--- Running: Sync (Parse only) Benchmarks ---
  cisv (sync) x 120,816 ops/sec ±4.48% (68 runs sampled)
    Speed: 54.27 MB/s | Avg Time: 0.01 ms | Ops/sec: 120816
    (cooling down...)

  csv-parse (sync) x 30,106 ops/sec ±2.87% (87 runs sampled)
    Speed: 13.52 MB/s | Avg Time: 0.03 ms | Ops/sec: 30106
    (cooling down...)

  papaparse (sync) x 55,327 ops/sec ±1.43% (95 runs sampled)
    Speed: 24.85 MB/s | Avg Time: 0.02 ms | Ops/sec: 55327
    (cooling down...)


>>>>> Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
  cisv (sync) x 57,755 ops/sec ±0.44% (95 runs sampled)
    Speed: 25.94 MB/s | Avg Time: 0.02 ms | Ops/sec: 57755
    (cooling down...)

  csv-parse (sync) x 31,905 ops/sec ±0.61% (96 runs sampled)
    Speed: 14.33 MB/s | Avg Time: 0.03 ms | Ops/sec: 31905
    (cooling down...)

  papaparse (sync) x 56,011 ops/sec ±0.43% (95 runs sampled)
    Speed: 25.16 MB/s | Avg Time: 0.02 ms | Ops/sec: 56011
    (cooling down...)


>>>>> Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
  cisv (async/stream) x 153,475 ops/sec ±1.02% (71 runs sampled)
    Speed: 68.94 MB/s | Avg Time: 0.01 ms | Ops/sec: 153475
    (cooling down...)

  papaparse (async/stream) x 33,315 ops/sec ±3.23% (78 runs sampled)
    Speed: 14.96 MB/s | Avg Time: 0.03 ms | Ops/sec: 33315
    (cooling down...)

  neat-csv (async/promise) x 16,668 ops/sec ±3.24% (76 runs sampled)
    Speed: 7.49 MB/s | Avg Time: 0.06 ms | Ops/sec: 16668
    (cooling down...)


>>>>> Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
  cisv (async/stream) x 48,911 ops/sec ±1.36% (82 runs sampled)
    Speed: 21.97 MB/s | Avg Time: 0.02 ms | Ops/sec: 48911
    (cooling down...)

  papaparse (async/stream) x 35,469 ops/sec ±2.58% (83 runs sampled)
    Speed: 15.93 MB/s | Avg Time: 0.03 ms | Ops/sec: 35469
    (cooling down...)

  neat-csv (async/promise) x 17,121 ops/sec ±2.26% (80 runs sampled)
    Speed: 7.69 MB/s | Avg Time: 0.06 ms | Ops/sec: 17121
    (cooling down...)


>>>>> Fastest Async is cisv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 54.27        | 0.01          | 120816         |
| csv-parse (sync)   | 13.52        | 0.03          | 30106          |
| papaparse (sync)   | 24.85        | 0.02          | 55327          |
| cisv (sync)        | 25.94        | 0.02          | 57755          |
| csv-parse (sync)   | 14.33        | 0.03          | 31905          |
| papaparse (sync)   | 25.16        | 0.02          | 56011          |


### Asynchronous Results

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 68.94        | 0.01          | 153475         |
| papaparse (async/stream) | 14.96        | 0.03          | 33315          |
| neat-csv (async/promise) | 7.49         | 0.06          | 16668          |
| cisv (async/stream)      | 21.97        | 0.02          | 48911          |
| papaparse (async/stream) | 15.93        | 0.03          | 35469          |
| neat-csv (async/promise) | 7.69         | 0.06          | 17121          |


=====================================================================================
from npm :
=====================================================================================

## AFTER

> cisv@0.0.1 benchmark
> node benchmark/benchmark.js ./fixtures/data.csv

Starting benchmark with file: ./fixtures/data.csv
All tests will retrieve row index: 857010

File size: 0.00 MB
Warning: File has fewer than 857011 rows. Tests will run but may not retrieve data.


--- Running: Sync (Parse only) Benchmarks ---
  cisv (sync) x 135,349 ops/sec ±5.05% (73 runs sampled)
    Speed: 60.80 MB/s | Avg Time: 0.01 ms | Ops/sec: 135349
    (cooling down...)

  csv-parse (sync) x 32,656 ops/sec ±1.14% (96 runs sampled)
    Speed: 14.67 MB/s | Avg Time: 0.03 ms | Ops/sec: 32656
    (cooling down...)

  papaparse (sync) x 55,933 ops/sec ±1.55% (91 runs sampled)
    Speed: 25.12 MB/s | Avg Time: 0.02 ms | Ops/sec: 55933
    (cooling down...)


>>>>> Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Sync (Parse + Access) Benchmarks ---
  cisv (sync) x 131,372 ops/sec ±3.48% (78 runs sampled)
    Speed: 59.01 MB/s | Avg Time: 0.01 ms | Ops/sec: 131372
    (cooling down...)

  csv-parse (sync) x 31,761 ops/sec ±1.21% (93 runs sampled)
    Speed: 14.27 MB/s | Avg Time: 0.03 ms | Ops/sec: 31761
    (cooling down...)

  papaparse (sync) x 54,544 ops/sec ±0.50% (96 runs sampled)
    Speed: 24.50 MB/s | Avg Time: 0.02 ms | Ops/sec: 54544
    (cooling down...)


>>>>> Fastest Sync is cisv (sync)

--------------------------------------------------

--- Running: Async (Parse only) Benchmarks ---
  cisv (async/stream) x 155,400 ops/sec ±1.79% (68 runs sampled)
    Speed: 69.80 MB/s | Avg Time: 0.01 ms | Ops/sec: 155400
    (cooling down...)

  papaparse (async/stream) x 32,404 ops/sec ±3.89% (75 runs sampled)
    Speed: 14.56 MB/s | Avg Time: 0.03 ms | Ops/sec: 32404
    (cooling down...)

  neat-csv (async/promise) x 16,670 ops/sec ±2.47% (76 runs sampled)
    Speed: 7.49 MB/s | Avg Time: 0.06 ms | Ops/sec: 16670
    (cooling down...)


>>>>> Fastest Async is cisv (async/stream)

--------------------------------------------------

--- Running: Async (Parse + Access) Benchmarks ---
  cisv (async/stream) x 107,916 ops/sec ±1.13% (75 runs sampled)
    Speed: 48.47 MB/s | Avg Time: 0.01 ms | Ops/sec: 107916
    (cooling down...)

  papaparse (async/stream) x 34,477 ops/sec ±2.88% (75 runs sampled)
    Speed: 15.49 MB/s | Avg Time: 0.03 ms | Ops/sec: 34477
    (cooling down...)

  neat-csv (async/promise) x 17,038 ops/sec ±2.34% (81 runs sampled)
    Speed: 7.65 MB/s | Avg Time: 0.06 ms | Ops/sec: 17038
    (cooling down...)


>>>>> Fastest Async is cisv (async/stream)

--------------------------------------------------

Benchmark Results Table (Markdown)

### Synchronous Results

| Library            | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------|--------------|---------------|----------------|
| cisv (sync)        | 60.80        | 0.01          | 135349         |
| csv-parse (sync)   | 14.67        | 0.03          | 32656          |
| papaparse (sync)   | 25.12        | 0.02          | 55933          |
| cisv (sync)        | 59.01        | 0.01          | 131372         |
| csv-parse (sync)   | 14.27        | 0.03          | 31761          |
| papaparse (sync)   | 24.50        | 0.02          | 54544          |


### Asynchronous Results

| Library                  | Speed (MB/s) | Avg Time (ms) | Operations/sec |
|--------------------------|--------------|---------------|----------------|
| cisv (async/stream)      | 69.80        | 0.01          | 155400         |
| papaparse (async/stream) | 14.56        | 0.03          | 32404          |
| neat-csv (async/promise) | 7.49         | 0.06          | 16670          |
| cisv (async/stream)      | 48.47        | 0.01          | 107916         |
| papaparse (async/stream) | 15.49        | 0.03          | 34477          |
| neat-csv (async/promise) | 7.65         | 0.06          | 17038          |

