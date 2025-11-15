## BENCHMARK RESULTS
```
**DATE:** Sat Nov 15 22:18:49 UTC 2025
**COMMIT:** 248ab55e753cd5a839f4907d4597c381cf8549ee
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
  Run 1: 0.0217733s
  Run 2: 0.0203111s
  Run 3: 0.0202143s
  Average Time: 0.0208 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.188477s
  Run 2: 0.189259s
  Run 3: 0.177363s
  Average Time: 0.1850 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.021579s
  Run 2: 0.0213945s
  Run 3: 0.0214257s
  Average Time: 0.0215 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.43545s
  Run 2: 2.30144s
  Run 3: 2.29134s
  Average Time: 2.3427 seconds
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
| wc -l                |      5032.09 |       0.0215 |          46.51 |
| rust-csv             |       584.81 |       0.1850 |           5.41 |
| csvkit               |        46.18 |       2.3427 |           0.43 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5201.44 |       0.0208 |          48.08 |
| wc -l                |      5032.09 |       0.0215 |          46.51 |
| rust-csv             |       584.81 |       0.1850 |           5.41 |
| csvkit               |        46.18 |       2.3427 |           0.43 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.436717s
  Run 2: 0.431018s
  Run 3: 0.434706s
  Average Time: 0.4341 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.320278s
  Run 2: 0.317139s
  Run 3: 0.321364s
  Average Time: 0.3196 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.60245s
  Run 2: 2.56421s
  Run 3: 2.58601s
  Average Time: 2.5842 seconds
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
| rust-csv             |       338.52 |       0.3196 |           3.13 |
| cisv                 |       249.23 |       0.4341 |           2.30 |
| csvkit               |        41.87 |       2.5842 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       338.52 |       0.3196 |           3.13 |
| cisv                 |       249.23 |       0.4341 |           2.30 |
| csvkit               |        41.87 |       2.5842 |           0.39 |

## NPM Benchmarks

npm error code ENOENT
npm error syscall open
npm error path /home/runner/work/cisv/cisv/package.json
npm error errno -2
npm error enoent Could not read package.json: Error: ENOENT: no such file or directory, open '/home/runner/work/cisv/cisv/package.json'
npm error enoent This is related to npm not being able to find a file.
npm error enoent
npm error A complete log of this run can be found in: /home/runner/.npm/_logs/2025-11-15T22_18_49_244Z-debug-0.log

Cleaning up test files...
Removed large.csv

Benchmark complete!

