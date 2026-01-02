## BENCHMARK RESULTS
```
DATE: Fri Jan  2 15:34:48 UTC 2026
COMMIT: dc8c9059bc7baaf617d0c5214dc0c9e8ae28b9a9
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
  Run 1 failed with exit code 127
  Run 2 failed with exit code 127
  Run 3 failed with exit code 127
  All runs failed for cisv

-> rust-csv
  Run 1: 0.184246s
  Run 2: 0.174272s
  Run 3: 0.186347s
  Average Time: 0.1816 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0216413s
  Run 2: 0.0213625s
  Run 3: 0.02126s
  Average Time: 0.0214 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.42022s
  Run 2: 2.23986s
  Run 3: 2.2351s
  Average Time: 2.2984 seconds
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
| wc -l                |      5055.61 |       0.0214 |          46.73 |
| rust-csv             |       595.76 |       0.1816 |           5.51 |
| csvkit               |        47.07 |       2.2984 |           0.44 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| wc -l                |      5055.61 |       0.0214 |          46.73 |
| rust-csv             |       595.76 |       0.1816 |           5.51 |
| csvkit               |        47.07 |       2.2984 |           0.44 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1 failed with exit code 127
  Run 2 failed with exit code 127
  Run 3 failed with exit code 127
  All runs failed for cisv

-> rust-csv
  Run 1: 0.319664s
  Run 2: 0.314059s
  Run 3: 0.325891s
  Average Time: 0.3199 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.58016s
  Run 2: 2.54296s
  Run 3: 2.56814s
  Average Time: 2.5638 seconds
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
| rust-csv             |       338.20 |       0.3199 |           3.13 |
| csvkit               |        42.20 |       2.5638 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       338.20 |       0.3199 |           3.13 |
| csvkit               |        42.20 |       2.5638 |           0.39 |

## NPM Benchmarks

Error: package.json not found or npm not installed

