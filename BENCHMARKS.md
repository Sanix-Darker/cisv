## BENCHMARK RESULTS
```
DATE: Sat Nov 15 22:30:01 UTC 2025
COMMIT: f2853ed3d03c1398c5be11853de9ac28f3fb3c0c
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
  Run 1: 0.0226421s
  Run 2: 0.020613s
  Run 3: 0.0201261s
  Average Time: 0.0211 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.189707s
  Run 2: 0.188575s
  Run 3: 0.175647s
  Average Time: 0.1846 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0216455s
  Run 2: 0.0214756s
  Run 3: 0.0215247s
  Average Time: 0.0215 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.38741s
  Run 2: 2.2831s
  Run 3: 2.27701s
  Average Time: 2.3158 seconds
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
| wc -l                |      5032.09 |       0.0215 |          46.51 |
| rust-csv             |       586.08 |       0.1846 |           5.42 |
| csvkit               |        46.72 |       2.3158 |           0.43 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5127.49 |       0.0211 |          47.39 |
| wc -l                |      5032.09 |       0.0215 |          46.51 |
| rust-csv             |       586.08 |       0.1846 |           5.42 |
| csvkit               |        46.72 |       2.3158 |           0.43 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.436359s
  Run 2: 0.433794s
  Run 3: 0.431139s
  Average Time: 0.4338 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.319634s
  Run 2: 0.323812s
  Run 3: 0.320839s
  Average Time: 0.3214 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.57306s
  Run 2: 2.62427s
  Run 3: 2.54828s
  Average Time: 2.5819 seconds
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
| rust-csv             |       336.62 |       0.3214 |           3.11 |
| cisv                 |       249.40 |       0.4338 |           2.31 |
| csvkit               |        41.90 |       2.5819 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       336.62 |       0.3214 |           3.11 |
| cisv                 |       249.40 |       0.4338 |           2.31 |
| csvkit               |        41.90 |       2.5819 |           0.39 |

## NPM Benchmarks


> cisv@0.0.7 install
> node-gyp rebuild

gyp info it worked if it ends with ok
gyp info using node-gyp@11.0.0
gyp info using node@23.11.1 | linux | x64
gyp info find Python using Python version 3.12.3 found at "/usr/bin/python3"

gyp info spawn /usr/bin/python3
gyp info spawn args [
gyp info spawn args '/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp/gyp/gyp_main.py',
gyp info spawn args 'binding.gyp',
gyp info spawn args '-f',
gyp info spawn args 'make',
gyp info spawn args '-I',
gyp info spawn args '/home/runner/work/cisv/cisv/npm/build/config.gypi',
gyp info spawn args '-I',
gyp info spawn args '/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp/addon.gypi',
gyp info spawn args '-I',
gyp info spawn args '/home/runner/.cache/node-gyp/23.11.1/include/node/common.gypi',
gyp info spawn args '-Dlibrary=shared_library',
gyp info spawn args '-Dvisibility=default',
gyp info spawn args '-Dnode_root_dir=/home/runner/.cache/node-gyp/23.11.1',
gyp info spawn args '-Dnode_gyp_dir=/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp',
gyp info spawn args '-Dnode_lib_file=/home/runner/.cache/node-gyp/23.11.1/<(target_arch)/node.lib',
gyp info spawn args '-Dmodule_root_dir=/home/runner/work/cisv/cisv/npm',
gyp info spawn args '-Dnode_engine=v8',
gyp info spawn args '--depth=.',
gyp info spawn args '--no-parallel',
gyp info spawn args '--generator-output',
gyp info spawn args 'build',
gyp info spawn args '-Goutput_dir=.'
gyp info spawn args ]
gyp info spawn make
gyp info spawn args [ 'BUILDTYPE=Release', '-C', 'build' ]
make: Entering directory '/home/runner/work/cisv/cisv/npm/build'
  CC(target) Release/obj.target/nothing/node_modules/node-addon-api/nothing.o
rm -f Release/obj.target/node_modules/node-addon-api/nothing.a Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list; mkdir -p `dirname Release/obj.target/node_modules/node-addon-api/nothing.a`
ar crs Release/obj.target/node_modules/node-addon-api/nothing.a @Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list
  COPY Release/nothing.a
  CXX(target) Release/obj.target/cisv/cisv/cisv_addon.o
  CC(target) Release/obj.target/cisv/../lib/cisv_parser.o
  CC(target) Release/obj.target/cisv/../lib/cisv_writer.o
  CC(target) Release/obj.target/cisv/../lib/cisv_transformer.o
  SOLINK_MODULE(target) Release/obj.target/cisv.node
  COPY Release/cisv.node
make: Leaving directory '/home/runner/work/cisv/cisv/npm/build'
gyp info ok 

up to date, audited 262 packages in 5s

46 packages are looking for funding
  run `npm fund` for details

3 moderate severity vulnerabilities

To address all issues (including breaking changes), run:
  npm audit fix --force

Run `npm audit` for details.

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

