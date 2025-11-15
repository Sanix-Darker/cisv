## BENCHMARK RESULTS
```
DATE: Sat Nov 15 23:12:29 UTC 2025
COMMIT: 752907ed67518d2a566eb1686cdd1312d81cf7dc
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
  Run 1: 0.0219536s
  Run 2: 0.0210238s
  Run 3: 0.0209265s
  Average Time: 0.0213 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.183442s
  Run 2: 0.188875s
  Run 3: 0.189518s
  Average Time: 0.1873 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.02283s
  Run 2: 0.0224392s
  Run 3: 0.0219481s
  Average Time: 0.0224 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.42971s
  Run 2: 2.34041s
  Run 3: 2.34619s
  Average Time: 2.3721 seconds
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
| cisv                 |      5079.34 |       0.0213 |          46.95 |
| wc -l                |      4829.91 |       0.0224 |          44.64 |
| rust-csv             |       577.63 |       0.1873 |           5.34 |
| csvkit               |        45.61 |       2.3721 |           0.42 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5079.34 |       0.0213 |          46.95 |
| wc -l                |      4829.91 |       0.0224 |          44.64 |
| rust-csv             |       577.63 |       0.1873 |           5.34 |
| csvkit               |        45.61 |       2.3721 |           0.42 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.459149s
  Run 2: 0.432704s
  Run 3: 0.432262s
  Average Time: 0.4414 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.321084s
  Run 2: 0.331566s
  Run 3: 0.326894s
  Average Time: 0.3265 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.57627s
  Run 2: 2.59093s
  Run 3: 2.63323s
  Average Time: 2.6001 seconds
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
| rust-csv             |       331.36 |       0.3265 |           3.06 |
| cisv                 |       245.11 |       0.4414 |           2.27 |
| csvkit               |        41.61 |       2.6001 |           0.38 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       331.36 |       0.3265 |           3.06 |
| cisv                 |       245.11 |       0.4414 |           2.27 |
| csvkit               |        41.61 |       2.6001 |           0.38 |

## NPM Benchmarks


> cisv@0.0.7 benchmark-js
> node ./benchmark.js

Failed to load cisvParser from the specified paths.
 Using benchmark file: /home/runner/work/cisv/cisv/fixtures/data.csv
Starting benchmark with file: /home/runner/work/cisv/cisv/fixtures/data.csv
All tests will retrieve row index: 4

File size: 0.00 MB
Sample of target row: [ '4', 'Dana White', 'dana.white@email.com', 'Chicago' ]


--- Running: Sync (Parse only) Benchmarks ---
```
  cisv (sync): 
    Speed: 0.00 MB/s | Avg Time: 0.00 ms | Ops/sec: 0
    (cooling down...)

```
```
  csv-parse (sync) x 39,806 ops/sec ±0.83% (99 runs sampled)
    Speed: 18.41 MB/s | Avg Time: 0.03 ms | Ops/sec: 39806
    (cooling down...)

```
```
  papaparse (sync) x 60,796 ops/sec ±1.42% (93 runs sampled)
    Speed: 28.12 MB/s | Avg Time: 0.02 ms | Ops/sec: 60796
    (cooling down...)

```
```
  udsv (sync) x 152,948 ops/sec ±0.39% (97 runs sampled)
    Speed: 70.74 MB/s | Avg Time: 0.01 ms | Ops/sec: 152948
    (cooling down...)

```
```
  d3-dsv (sync) x 210,703 ops/sec ±0.24% (98 runs sampled)
    Speed: 97.46 MB/s | Avg Time: 0.00 ms | Ops/sec: 210703
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

A benchmark test failed and was caught: Event {
  timeStamp: 1763248326798,
  type: 'error',
  target: Benchmark {
    name: 'cisv (sync)',
    options: {
      async: false,
      defer: false,
      delay: 0.005,
      id: undefined,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      name: undefined,
      onAbort: undefined,
      onComplete: undefined,
      onCycle: undefined,
      onError: undefined,
      onReset: undefined,
      onStart: undefined
    },
    async: false,
    defer: false,
    delay: 0.005,
    initCount: 1,
    maxTime: 5,
    minSamples: 5,
    minTime: 0.05,
    id: 1,
    fn: [Function (anonymous)],
    stats: {
      moe: 0,
      rme: 0,
      sem: 0,
      deviation: 0,
      mean: 0,
      sample: [],
      variance: 0
    },
    times: { cycle: 0, elapsed: 0, period: 0, timeStamp: 0 },
    running: false,
    count: 0,
    compiled: [Function: anonymous],
    f17632483267361: [Function (anonymous)],
    cycles: 0,
    error: TypeError: cisvParser is not a constructor
        at Benchmark.<anonymous> (/home/runner/work/cisv/cisv/npm/benchmark.js:113:28)
        at Benchmark.eval (eval at createCompiled (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1725:16), <anonymous>:5:124)
        at clock (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1644:22)
        at clock (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1818:20)
        at cycle (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2007:49)
        at Benchmark.run (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2114:13)
        at execute (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:860:74)
        at invoke (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:970:20)
        at compute (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1966:7)
        at Benchmark.run (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2119:11),
    aborted: true
  },
  currentTarget: Suite {
    '0': Benchmark {
      name: 'cisv (sync)',
      options: [Object],
      async: false,
      defer: false,
      delay: 0.005,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      id: 1,
      fn: [Function (anonymous)],
      stats: [Object],
      times: [Object],
      running: false,
      count: 0,
      compiled: [Function: anonymous],
      f17632483267361: [Function (anonymous)],
      cycles: 0,
      error: TypeError: cisvParser is not a constructor
          at Benchmark.<anonymous> (/home/runner/work/cisv/cisv/npm/benchmark.js:113:28)
          at Benchmark.eval (eval at createCompiled (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1725:16), <anonymous>:5:124)
          at clock (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1644:22)
          at clock (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1818:20)
          at cycle (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2007:49)
          at Benchmark.run (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2114:13)
          at execute (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:860:74)
          at invoke (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:970:20)
          at compute (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:1966:7)
          at Benchmark.run (/home/runner/work/cisv/cisv/npm/node_modules/benchmark/benchmark.js:2119:11),
      aborted: true
    },
    '1': Benchmark {
      name: 'csv-parse (sync)',
      options: [Object],
      async: false,
      defer: false,
      delay: 0.005,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      id: 2,
      fn: [Function (anonymous)],
      stats: [Object],
      times: [Object],
      running: false,
      count: 2035,
      compiled: [Function: anonymous],
      cycles: 7,
      hz: 39805.58822284531
    },
    '2': Benchmark {
      name: 'papaparse (sync)',
      options: [Object],
      async: false,
      defer: false,
      delay: 0.005,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      id: 3,
      fn: [Function (anonymous)],
      stats: [Object],
      times: [Object],
      running: false,
      count: 3212,
      compiled: [Function: anonymous],
      cycles: 6,
      hz: 60795.64608013495
    },
    '3': Benchmark {
      name: 'udsv (sync)',
      options: [Object],
      async: false,
      defer: false,
      delay: 0.005,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      id: 4,
      fn: [Function (anonymous)],
      stats: [Object],
      times: [Object],
      running: false,
      count: 7787,
      compiled: [Function: anonymous],
      cycles: 8,
      hz: 152947.67502081575
    },
    '4': Benchmark {
      name: 'd3-dsv (sync)',
      options: [Object],
      async: false,
      defer: false,
      delay: 0.005,
      initCount: 1,
      maxTime: 5,
      minSamples: 5,
      minTime: 0.05,
      id: 5,
      fn: [Function (anonymous)],
      stats: [Object],
      times: [Object],
      running: false,
      count: 10705,
      compiled: [Function: anonymous],
      cycles: 7,
      hz: 210703.3372539758
    },
    name: 'Sync (Parse only) Benchmark',
    options: { name: undefined },
    length: 5,
    events: { cycle: [Array], error: [Array], complete: [Array] },
    running: false
  },
  result: undefined
}

Cleaning up test files...

Benchmark complete!

