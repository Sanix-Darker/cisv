## BENCHMARK RESULTS
```
DATE: Fri Jan  2 15:42:35 UTC 2026
COMMIT: fb9fde440938133d6249ed022ebd3f1806145f81
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
Building cisv...
make -C core clean
make[1]: Entering directory '/home/runner/work/cisv/cisv/core'
rm -rf build
make[1]: Leaving directory '/home/runner/work/cisv/cisv/core'
make -C cli clean
make[1]: Entering directory '/home/runner/work/cisv/cisv/cli'
rm -rf build
make[1]: Leaving directory '/home/runner/work/cisv/cisv/cli'
cd bindings/nodejs && rm -rf build node_modules 2>/dev/null || true
make -C bindings/python clean 2>/dev/null || true
make[1]: Entering directory '/home/runner/work/cisv/cisv/bindings/python'
rm -rf build/ dist/ *.egg-info/
find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
find . -type f -name "*.pyc" -delete 2>/dev/null || true
make[1]: Leaving directory '/home/runner/work/cisv/cisv/bindings/python'
make -C bindings/php clean 2>/dev/null || true
make[1]: Entering directory '/home/runner/work/cisv/cisv/bindings/php'
make[1]: Leaving directory '/home/runner/work/cisv/cisv/bindings/php'
rm -rf build_cmake
Building core library...
make -C core all
make[1]: Entering directory '/home/runner/work/cisv/cisv/core'
mkdir -p build
cc -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -fPIC -D_GNU_SOURCE -Iinclude -c -o build/parser.o src/parser.c
cc -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -fPIC -D_GNU_SOURCE -Iinclude -c -o build/writer.o src/writer.c
cc -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -fPIC -D_GNU_SOURCE -Iinclude -c -o build/transformer.o src/transformer.c
ar rcs build/libcisv.a build/parser.o build/writer.o build/transformer.o
cc -shared -o build/libcisv.so build/parser.o build/writer.o build/transformer.o -flto
make[1]: Leaving directory '/home/runner/work/cisv/cisv/core'
Building CLI...
make -C cli all
make[1]: Entering directory '/home/runner/work/cisv/cisv/cli'
mkdir -p build
cc -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -D_GNU_SOURCE -I../core/include -c -o build/main.o src/main.c
cc -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -D_GNU_SOURCE -o build/cisv build/main.o -L../core/build -lcisv -flto -s -lrt -lm -lpthread
make[1]: Leaving directory '/home/runner/work/cisv/cisv/cli'
✓ cisv built successfully

## CLI BENCHMARKS


### Testing with large.csv

File info: 108.19 MB, 1000001 rows

#### ROW COUNTING TEST

```


-> cisv
  Run 1: 0.0219569s
  Run 2: 0.0214541s
  Run 3: 0.0205281s
  Average Time: 0.0213 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.186172s
  Run 2: 0.185547s
  Run 3: 0.185892s
  Average Time: 0.1859 seconds
  Successful runs: 3/3

-> wc -l
  Run 1: 0.0225298s
  Run 2: 0.021668s
  Run 3: 0.0217144s
  Average Time: 0.0220 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.30889s
  Run 2: 2.30377s
  Run 3: 2.28254s
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
| cisv                 |      5079.34 |       0.0213 |          46.95 |
| wc -l                |      4917.73 |       0.0220 |          45.45 |
| rust-csv             |       581.98 |       0.1859 |           5.38 |
| csvkit               |        47.07 |       2.2984 |           0.44 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| cisv                 |      5079.34 |       0.0213 |          46.95 |
| wc -l                |      4917.73 |       0.0220 |          45.45 |
| rust-csv             |       581.98 |       0.1859 |           5.38 |
| csvkit               |        47.07 |       2.2984 |           0.44 |

#### COLUMN SELECTION TEST (COLUMNS 0,2,3)

```


-> cisv
  Run 1: 0.438565s
  Run 2: 0.430944s
  Run 3: 0.431259s
  Average Time: 0.4336 seconds
  Successful runs: 3/3

-> rust-csv
  Run 1: 0.324164s
  Run 2: 0.312068s
  Run 3: 0.325021s
  Average Time: 0.3204 seconds
  Successful runs: 3/3

-> csvkit
  Run 1: 2.5428s
  Run 2: 2.53975s
  Run 3: 2.54876s
  Average Time: 2.5438 seconds
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
| rust-csv             |       337.67 |       0.3204 |           3.12 |
| cisv                 |       249.52 |       0.4336 |           2.31 |
| csvkit               |        42.53 |       2.5438 |           0.39 |


Sorted by Operations/sec - Most Operations First:
| Library              | Speed (MB/s) | Avg Time (s) | Operations/sec |
|----------------------|--------------|--------------|----------------|
| rust-csv             |       337.67 |       0.3204 |           3.12 |
| cisv                 |       249.52 |       0.4336 |           2.31 |
| csvkit               |        42.53 |       2.5438 |           0.39 |

## NPM Benchmarks

npm warn deprecated inflight@1.0.6: This module is not supported, and leaks memory. Do not use it. Check out lru-cache if you want a good and tested way to coalesce async requests by a key value, which is much more comprehensive and powerful.
npm warn deprecated rimraf@3.0.2: Rimraf versions prior to v4 are no longer supported
npm warn deprecated glob@7.2.3: Glob versions prior to v9 are no longer supported
npm warn deprecated glob@7.2.3: Glob versions prior to v9 are no longer supported
npm warn deprecated glob@7.2.3: Glob versions prior to v9 are no longer supported

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
gyp info spawn args '/home/runner/work/cisv/cisv/bindings/nodejs/build/config.gypi',
gyp info spawn args '-I',
gyp info spawn args '/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp/addon.gypi',
gyp info spawn args '-I',
gyp info spawn args '/home/runner/.cache/node-gyp/23.11.1/include/node/common.gypi',
gyp info spawn args '-Dlibrary=shared_library',
gyp info spawn args '-Dvisibility=default',
gyp info spawn args '-Dnode_root_dir=/home/runner/.cache/node-gyp/23.11.1',
gyp info spawn args '-Dnode_gyp_dir=/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp',
gyp info spawn args '-Dnode_lib_file=/home/runner/.cache/node-gyp/23.11.1/<(target_arch)/node.lib',
gyp info spawn args '-Dmodule_root_dir=/home/runner/work/cisv/cisv/bindings/nodejs',
gyp info spawn args '-Dnode_engine=v8',
gyp info spawn args '--depth=.',
gyp info spawn args '--no-parallel',
gyp info spawn args '--generator-output',
gyp info spawn args 'build',
gyp info spawn args '-Goutput_dir=.'
gyp info spawn args ]
gyp info spawn make
gyp info spawn args [ 'BUILDTYPE=Release', '-C', 'build' ]
make: Entering directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
  CC(target) Release/obj.target/nothing/node_modules/node-addon-api/nothing.o
rm -f Release/obj.target/node_modules/node-addon-api/nothing.a Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list; mkdir -p `dirname Release/obj.target/node_modules/node-addon-api/nothing.a`
ar crs Release/obj.target/node_modules/node-addon-api/nothing.a @Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list
  COPY Release/nothing.a
  CXX(target) Release/obj.target/cisv/cisv/cisv_addon.o
  CC(target) Release/obj.target/cisv/../../core/src/parser.o
  CC(target) Release/obj.target/cisv/../../core/src/writer.o
  CC(target) Release/obj.target/cisv/../../core/src/transformer.o
  SOLINK_MODULE(target) Release/obj.target/cisv.node
  COPY Release/cisv.node
make: Leaving directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
gyp info ok 

added 261 packages, and audited 262 packages in 6s

46 packages are looking for funding
  run `npm fund` for details

2 vulnerabilities (1 moderate, 1 high)

To address all issues, run:
  npm audit fix

Run `npm audit` for details.

> cisv@0.0.7 build
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
gyp info spawn args '/home/runner/work/cisv/cisv/bindings/nodejs/build/config.gypi',
gyp info spawn args '-I',
gyp info spawn args '/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp/addon.gypi',
gyp info spawn args '-I',
gyp info spawn args '/home/runner/.cache/node-gyp/23.11.1/include/node/common.gypi',
gyp info spawn args '-Dlibrary=shared_library',
gyp info spawn args '-Dvisibility=default',
gyp info spawn args '-Dnode_root_dir=/home/runner/.cache/node-gyp/23.11.1',
gyp info spawn args '-Dnode_gyp_dir=/opt/hostedtoolcache/node/23.11.1/x64/lib/node_modules/npm/node_modules/node-gyp',
gyp info spawn args '-Dnode_lib_file=/home/runner/.cache/node-gyp/23.11.1/<(target_arch)/node.lib',
gyp info spawn args '-Dmodule_root_dir=/home/runner/work/cisv/cisv/bindings/nodejs',
gyp info spawn args '-Dnode_engine=v8',
gyp info spawn args '--depth=.',
gyp info spawn args '--no-parallel',
gyp info spawn args '--generator-output',
gyp info spawn args 'build',
gyp info spawn args '-Goutput_dir=.'
gyp info spawn args ]
gyp info spawn make
gyp info spawn args [ 'BUILDTYPE=Release', '-C', 'build' ]
make: Entering directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
  CC(target) Release/obj.target/nothing/node_modules/node-addon-api/nothing.o
rm -f Release/obj.target/node_modules/node-addon-api/nothing.a Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list; mkdir -p `dirname Release/obj.target/node_modules/node-addon-api/nothing.a`
ar crs Release/obj.target/node_modules/node-addon-api/nothing.a @Release/obj.target/node_modules/node-addon-api/nothing.a.ar-file-list
  COPY Release/nothing.a
  CXX(target) Release/obj.target/cisv/cisv/cisv_addon.o
  CC(target) Release/obj.target/cisv/../../core/src/parser.o
  CC(target) Release/obj.target/cisv/../../core/src/writer.o
  CC(target) Release/obj.target/cisv/../../core/src/transformer.o
  SOLINK_MODULE(target) Release/obj.target/cisv.node
  COPY Release/cisv.node
make: Leaving directory '/home/runner/work/cisv/cisv/bindings/nodejs/build'
gyp info ok 

> cisv@0.0.7 benchmark-js
> node ./benchmark.js

Failed to locate benchmark file in either specified path or '../fixtures': Error: File not found
    at Object.<anonymous> (/home/runner/work/cisv/cisv/bindings/nodejs/benchmark.js:29:13)
    at Module._compile (node:internal/modules/cjs/loader:1734:14)
    at Object..js (node:internal/modules/cjs/loader:1899:10)
    at Module.load (node:internal/modules/cjs/loader:1469:32)
    at Function._load (node:internal/modules/cjs/loader:1286:12)
    at TracingChannel.traceSync (node:diagnostics_channel:322:14)
    at wrapModuleLoad (node:internal/modules/cjs/loader:235:24)
    at Function.executeUserEntryPoint [as runMain] (node:internal/modules/run_main:151:5)
    at node:internal/main/run_main_module:33:47
/home/runner/work/cisv/cisv/bindings/nodejs/benchmark.js:34
  throw error;
  ^

Error: File not found
    at Object.<anonymous> (/home/runner/work/cisv/cisv/bindings/nodejs/benchmark.js:29:13)
    at Module._compile (node:internal/modules/cjs/loader:1734:14)
    at Object..js (node:internal/modules/cjs/loader:1899:10)
    at Module.load (node:internal/modules/cjs/loader:1469:32)
    at Function._load (node:internal/modules/cjs/loader:1286:12)
    at TracingChannel.traceSync (node:diagnostics_channel:322:14)
    at wrapModuleLoad (node:internal/modules/cjs/loader:235:24)
    at Function.executeUserEntryPoint [as runMain] (node:internal/modules/run_main:151:5)
    at node:internal/main/run_main_module:33:47

Node.js v23.11.1

Cleaning up test files...
Removed large.csv

Benchmark complete!

