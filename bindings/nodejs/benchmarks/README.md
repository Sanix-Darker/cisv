# CISV Node.js Benchmark

Docker benchmark for the Node.js binding against common JS CSV parsers.

## Compared Libraries

- `cisv`
- `cisv-iterator`
- `udsv`
- `papaparse`
- `csv-parse`
- `d3-dsv`
- `fast-csv`
- `neat-csv`

## Build

```bash
docker build -t cisv-node-bench -f bindings/nodejs/benchmarks/Dockerfile .
```

## Run

```bash
# Default dataset
# (100k rows, 5 iterations)
docker run --rm --cpus=2 --memory=4g cisv-node-bench

# Custom size
docker run --rm --cpus=2 --memory=4g cisv-node-bench \
  --rows=200000 --cols=5 --iterations=3

# Skip slower parsers
docker run --rm --cpus=2 --memory=4g cisv-node-bench --fast

# Benchmark an existing file
docker run --rm --cpus=2 --memory=4g \
  -v /path/to/data:/data cisv-node-bench --file=/data/large.csv
```

## CLI Options

- `--rows=N` (default: `100000`)
- `--cols=N` (default: `5`)
- `--file=PATH`
- `--iterations=N` (default: `5`)
- `--fast`

## Notes

- `cisv` in this benchmark measures full parse + JS marshaling cost (`string[][]`).
- `cisv-iterator` measures row-by-row streaming overhead through `openIterator`/`fetchRow`.
- Results are printed as average time and throughput (MB/s).
- Run with fixed CPU/memory limits to reduce noise across machines.
