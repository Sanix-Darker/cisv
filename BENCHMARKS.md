## BENCHMARK RESULTS
```
DATE: Sat Nov 15 22:26:30 UTC 2025
COMMIT: f20141f7c334bcafe5bff3a69282d3d0c739c1ed
```
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
  Run 1: 0.0217245s
  Run 2: 0.0206788s
  Run 3: 0.0209408s
  Average Time: 0.0211 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.192736s
  Run 2: 0.191062s
  Run 3: 0.188862s
  Average Time: 0.1909 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0222838s
  Run 2: 0.0226128s
  Run 3: 0.0213947s
  Average Time: 0.0221 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.35846s
  Run 2: 2.35629s
  Run 3: 2.3525s
  Average Time: 2.3558 seconds
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
| cisv                 |      5127.49 |       0.0211 |          47.39 |
| wc -l                |      4895.48 |       0.0221 |          45.25 |
| rust-csv             |       566.74 |       0.1909 |           5.24 |
| csvkit               |        45.92 |       2.3558 |           0.42 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5127.49 |       0.0211 |          47.39 |
| wc -l                |      4895.48 |       0.0221 |          45.25 |
| rust-csv             |       566.74 |       0.1909 |           5.24 |
| csvkit               |        45.92 |       2.3558 |           0.42 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.433977s
  Run 2: 0.440974s
  Run 3: 0.436573s
  Average Time: 0.4372 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.323612s
  Run 2: 0.331573s
  Run 3: 0.320407s
  Average Time: 0.3252 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.56045s
  Run 2: 2.61004s
  Run 3: 2.55851s
  Average Time: 2.5763 seconds
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
| rust-csv             |       332.69 |       0.3252 |           3.08 |
| cisv                 |       247.46 |       0.4372 |           2.29 |
| csvkit               |        41.99 |       2.5763 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       332.69 |       0.3252 |           3.08 |
| cisv                 |       247.46 |       0.4372 |           2.29 |
| csvkit               |        41.99 |       2.5763 |           0.39 |

## NPM Benchmarks


> cisv@0.0.7 benchmark-js
> node ../benchmark/benchmark.js

Failed to load cisvParser from the specified paths.
node:internal/modules/cjs/loader:1408
  throw err;
  ^

Error: Cannot find module 'csv-parse/sync'
Require stack:
- /home/runner/work/cisv/cisv/benchmark/benchmark.js
    at Function._resolveFilename (node:internal/modules/cjs/loader:1405:15)
    at defaultResolveImpl (node:internal/modules/cjs/loader:1061:19)
    at resolveForCJSWithHooks (node:internal/modules/cjs/loader:1066:22)
    at Function._load (node:internal/modules/cjs/loader:1215:37)
    at TracingChannel.traceSync (node:diagnostics_channel:322:14)
    at wrapModuleLoad (node:internal/modules/cjs/loader:235:24)
    at Module.require (node:internal/modules/cjs/loader:1491:12)
    at require (node:internal/modules/helpers:135:16)
    at Object.<anonymous> (/home/runner/work/cisv/cisv/benchmark/benchmark.js:32:33)
    at Module._compile (node:internal/modules/cjs/loader:1734:14) {
  code: 'MODULE_NOT_FOUND',
  requireStack: [ '/home/runner/work/cisv/cisv/benchmark/benchmark.js' ]
}

Node.js v23.11.1

Cleaning up test files...

Benchmark complete!

