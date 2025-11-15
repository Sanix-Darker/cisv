## BENCHMARK RESULTS
```
DATE: Sat Nov 15 23:00:17 UTC 2025
COMMIT: 7b9c6ee061b636cbd72d8ab08793197fb172b03c
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
  Run 1: 0.0225294s
  Run 2: 0.0219939s
  Run 3: 0.0220187s
  Average Time: 0.0222 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.20649s
  Run 2: 0.190939s
  Run 3: 0.190119s
  Average Time: 0.1958 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0222485s
  Run 2: 0.0222785s
  Run 3: 0.0226967s
  Average Time: 0.0224 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.4325s
  Run 2: 2.31402s
  Run 3: 2.33884s
  Average Time: 2.3618 seconds
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
| cisv                 |      4873.42 |       0.0222 |          45.05 |
| wc -l                |      4829.91 |       0.0224 |          44.64 |
| rust-csv             |       552.55 |       0.1958 |           5.11 |
| csvkit               |        45.81 |       2.3618 |           0.42 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      4873.42 |       0.0222 |          45.05 |
| wc -l                |      4829.91 |       0.0224 |          44.64 |
| rust-csv             |       552.55 |       0.1958 |           5.11 |
| csvkit               |        45.81 |       2.3618 |           0.42 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.438275s
  Run 2: 0.43885s
  Run 3: 0.433384s
  Average Time: 0.4368 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.32317s
  Run 2: 0.3172s
  Run 3: 0.321904s
  Average Time: 0.3208 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.57889s
  Run 2: 2.57856s
  Run 3: 2.57223s
  Average Time: 2.5766 seconds
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
| rust-csv             |       337.25 |       0.3208 |           3.12 |
| cisv                 |       247.69 |       0.4368 |           2.29 |
| csvkit               |        41.99 |       2.5766 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       337.25 |       0.3208 |           3.12 |
| cisv                 |       247.69 |       0.4368 |           2.29 |
| csvkit               |        41.99 |       2.5766 |           0.39 |

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
  csv-parse (sync) x 41,431 ops/sec ±1.04% (94 runs sampled)
    Speed: 19.16 MB/s | Avg Time: 0.02 ms | Ops/sec: 41431
    (cooling down...)

```
```
  papaparse (sync) x 71,348 ops/sec ±1.68% (98 runs sampled)
    Speed: 33.00 MB/s | Avg Time: 0.01 ms | Ops/sec: 71348
    (cooling down...)

```
```
  udsv (sync) x 153,993 ops/sec ±0.15% (95 runs sampled)
    Speed: 71.23 MB/s | Avg Time: 0.01 ms | Ops/sec: 153993
    (cooling down...)

```
```
  d3-dsv (sync) x 208,893 ops/sec ±0.24% (98 runs sampled)
    Speed: 96.62 MB/s | Avg Time: 0.00 ms | Ops/sec: 208893
    (cooling down...)

```

 Fastest Sync is d3-dsv (sync)

A benchmark test failed and was caught: Event {
  timeStamp: 1763247594968,
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
    f17632475949061: [Function (anonymous)],
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
      f17632475949061: [Function (anonymous)],
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
      count: 2111,
      compiled: [Function: anonymous],
      cycles: 5,
      hz: 41431.29202394313
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
      count: 3638,
      compiled: [Function: anonymous],
      cycles: 6,
      hz: 71347.98732517101
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
      count: 7816,
      compiled: [Function: anonymous],
      cycles: 8,
      hz: 153992.68187522367
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
      count: 10607,
      compiled: [Function: anonymous],
      cycles: 7,
      hz: 208893.03719415175
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

