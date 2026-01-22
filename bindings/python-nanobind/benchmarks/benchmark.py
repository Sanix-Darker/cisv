#!/usr/bin/env python3

"""
Benchmark comparison: cisv vs popular Python CSV parsers

Compared libraries:
- cisv: High-performance C parser with SIMD optimizations
- polars: Rust-based DataFrame library with Arrow backend
- pyarrow: Apache Arrow's CSV reader (columnar format)
- datatable: Fast C-based parser (similar to R's data.table)
- duckdb: SQL database with fast CSV import
- pandas: Popular DataFrame library
- stdlib csv: Python's built-in csv module

# Install dependencies
pip install --upgrade --no-cache-dir cisv pandas polars pyarrow datatable duckdb numpy

# Run benchmark with 1 million rows × 10 columns (~100MB file)
python scripts/benchmark_python.py --rows 1000000 --cols 10

# Run with fast mode (parallel parsing with numpy arrays)
python scripts/benchmark_python.py --rows 1000000 --fast

# Run with benchmark mode (raw parsing speed, no data marshaling)
python scripts/benchmark_python.py --rows 1000000 --benchmark

# Run all cisv modes for comparison
python scripts/benchmark_python.py --rows 1000000 --only-cisv

# Run with 10 million rows × 20 columns (~2GB file)
python scripts/benchmark_python.py --rows 10000000 --cols 20

# Use an existing CSV file
python scripts/benchmark_python.py --file /path/to/large.csv

# Include parallel cisv benchmark (list output)
python scripts/benchmark_python.py --rows 1000000 --parallel

Benchmark modes for cisv:
- cisv: Single-threaded, returns list[list[str]]
- cisv-parallel: Multi-threaded, returns list[list[str]]
- cisv-fast: Multi-threaded + numpy arrays (faster than list output)
- cisv-bench: Multi-threaded, no data marshaling (raw parsing speed)
"""

import os
import time
import tempfile
import argparse


def generate_csv(filepath: str, rows: int, cols: int) -> int:
    """Generate a test CSV file and return its size in bytes."""
    print(f"Generating CSV: {rows:,} rows × {cols} columns...")
    start = time.perf_counter()

    with open(filepath, 'w') as f:
        # Header
        header = ','.join([f'col{i}' for i in range(cols)])
        f.write(header + '\n')

        # Data rows
        for row_num in range(rows):
            row = ','.join([f'value_{row_num}_{i}' for i in range(cols)])
            f.write(row + '\n')

            if row_num > 0 and row_num % 1_000_000 == 0:
                print(f"  Generated {row_num:,} rows...")

    elapsed = time.perf_counter() - start
    size = os.path.getsize(filepath)
    print(f"  Done in {elapsed:.2f}s, file size: {size / (1024**2):.1f} MB")
    return size


def benchmark_cisv(filepath: str, parallel: bool = False, fast: bool = False, benchmark: bool = False) -> tuple:
    """Benchmark cisv parser."""
    try:
        import cisv
    except ImportError:
        return None, "cisv not installed"

    if benchmark:
        mode = "benchmark (parallel, no data)"
    elif fast:
        mode = "fast (parallel+numpy)"
    elif parallel:
        mode = "parallel"
    else:
        mode = "single-threaded"
    print(f"Benchmarking cisv ({mode})...")

    # Warm up
    cisv.count_rows(filepath)

    # Count rows (fast path) - only for non-parallel single mode
    count_time = None
    row_count = None
    if not parallel and not fast and not benchmark:
        start = time.perf_counter()
        row_count = cisv.count_rows(filepath)
        count_time = time.perf_counter() - start

    # Full parse - handle both old (ctypes) and new (nanobind) versions
    start = time.perf_counter()
    try:
        if benchmark:
            # Use the benchmark mode (parse only, no data marshaling)
            try:
                result = cisv.parse_file_benchmark(filepath)
                rows = result  # CisvBenchmarkResult object with len() support
            except AttributeError:
                return None, "benchmark mode not supported (old cisv version)"
        elif fast:
            # Use the ultra-fast numpy-based parsing
            try:
                result = cisv.parse_file_fast(filepath)
                rows = result  # CisvResult object with len() support
            except AttributeError:
                return None, "fast mode not supported (old cisv version)"
        elif parallel:
            rows = cisv.parse_file(filepath, parallel=True)
        else:
            rows = cisv.parse_file(filepath)
    except TypeError:
        # Old ctypes version doesn't support parallel parameter
        if parallel or fast or benchmark:
            return None, "parallel/fast/benchmark not supported (old cisv version)"
        rows = cisv.parse_file(filepath)
    parse_time = time.perf_counter() - start

    # Get row count and column count
    parse_rows = len(rows)
    if fast or benchmark:
        parse_cols = len(rows[0]) if parse_rows > 0 else 0
    else:
        parse_cols = len(rows[0]) if rows else 0

    return {
        'count_time': count_time,
        'count_rows': row_count,
        'parse_time': parse_time,
        'parse_rows': parse_rows,
        'parse_cols': parse_cols,
    }, None


def benchmark_pandas(filepath: str) -> tuple:
    """Benchmark pandas CSV reader."""
    try:
        import pandas as pd
    except ImportError:
        return None, "pandas not installed"

    print("Benchmarking pandas...")

    # Full parse
    start = time.perf_counter()
    df = pd.read_csv(filepath)
    parse_time = time.perf_counter() - start

    return {
        'count_time': None,  # pandas doesn't have a fast count
        'count_rows': len(df),
        'parse_time': parse_time,
        'parse_rows': len(df),
        'parse_cols': len(df.columns),
    }, None


def benchmark_polars(filepath: str) -> tuple:
    """Benchmark polars CSV reader."""
    try:
        import polars as pl
    except ImportError:
        return None, "polars not installed"

    print("Benchmarking polars...")

    # Full parse
    start = time.perf_counter()
    df = pl.read_csv(filepath)
    parse_time = time.perf_counter() - start

    return {
        'count_time': None,  # polars doesn't have a fast count
        'count_rows': len(df),
        'parse_time': parse_time,
        'parse_rows': len(df),
        'parse_cols': len(df.columns),
    }, None


def benchmark_stdlib(filepath: str) -> tuple:
    """Benchmark Python stdlib csv reader."""
    import csv

    print("Benchmarking stdlib csv...")

    start = time.perf_counter()
    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        rows = list(reader)
    parse_time = time.perf_counter() - start

    return {
        'count_time': None,
        'count_rows': len(rows) - 1,  # exclude header
        'parse_time': parse_time,
        'parse_rows': len(rows) - 1,
        'parse_cols': len(rows[0]) if rows else 0,
    }, None


def benchmark_pyarrow(filepath: str) -> tuple:
    """Benchmark PyArrow CSV reader."""
    try:
        import pyarrow.csv as pa_csv
    except ImportError:
        return None, "pyarrow not installed"

    print("Benchmarking pyarrow...")

    start = time.perf_counter()
    table = pa_csv.read_csv(filepath)
    parse_time = time.perf_counter() - start

    return {
        'count_time': None,
        'count_rows': table.num_rows,
        'parse_time': parse_time,
        'parse_rows': table.num_rows,
        'parse_cols': table.num_columns,
    }, None


def benchmark_datatable(filepath: str) -> tuple:
    """Benchmark datatable CSV reader."""
    try:
        import datatable as dt
    except ImportError:
        return None, "datatable not installed"
    except Exception as e:
        return None, f"datatable error: {e}"

    print("Benchmarking datatable...")

    try:
        start = time.perf_counter()
        frame = dt.fread(filepath)
        parse_time = time.perf_counter() - start

        return {
            'count_time': None,
            'count_rows': frame.nrows,
            'parse_time': parse_time,
            'parse_rows': frame.nrows,
            'parse_cols': frame.ncols,
        }, None
    except Exception as e:
        return None, f"datatable error: {e}"


def benchmark_duckdb(filepath: str) -> tuple:
    """Benchmark DuckDB CSV reader."""
    try:
        import duckdb
    except ImportError:
        return None, "duckdb not installed"

    print("Benchmarking duckdb...")

    start = time.perf_counter()
    conn = duckdb.connect(':memory:')
    result = conn.execute(f"SELECT * FROM read_csv_auto('{filepath}')").fetchall()
    parse_time = time.perf_counter() - start

    row_count = len(result)
    col_count = len(result[0]) if result else 0
    conn.close()

    return {
        'count_time': None,
        'count_rows': row_count,
        'parse_time': parse_time,
        'parse_rows': row_count,
        'parse_cols': col_count,
    }, None


def format_throughput(file_size: int, parse_time: float) -> str:
    """Calculate and format throughput in MB/s."""
    if parse_time > 0:
        mb_per_sec = (file_size / (1024**2)) / parse_time
        return f"{mb_per_sec:.1f} MB/s"
    return "N/A"


def main():
    parser = argparse.ArgumentParser(description='Benchmark CSV parsers')
    parser.add_argument('--rows', type=int, default=1_000_000, help='Number of rows')
    parser.add_argument('--cols', type=int, default=10, help='Number of columns')
    parser.add_argument('--file', type=str, help='Use existing CSV file instead of generating')
    parser.add_argument('--skip-stdlib', action='store_true', help='Skip stdlib csv benchmark')
    parser.add_argument('--parallel', action='store_true', help='Include parallel cisv benchmark')
    parser.add_argument('--fast', action='store_true', help='Include fast cisv benchmark (parallel+numpy)')
    parser.add_argument('--benchmark', action='store_true', help='Include benchmark mode (parallel, no data marshal)')
    parser.add_argument('--only-cisv', action='store_true', help='Only benchmark cisv (all modes)')
    args = parser.parse_args()

    # Generate or use existing file
    if args.file:
        filepath = args.file
        file_size = os.path.getsize(filepath)
        print(f"Using existing file: {filepath} ({file_size / (1024**2):.1f} MB)")
    else:
        filepath = tempfile.mktemp(suffix='.csv')
        file_size = generate_csv(filepath, args.rows, args.cols)

    print(f"\n{'='*60}")
    print(f"BENCHMARK: {args.rows:,} rows × {args.cols} columns")
    print(f"File size: {file_size / (1024**2):.1f} MB")
    print(f"{'='*60}\n")

    results = {}

    # Run cisv benchmarks
    results['cisv'], err = benchmark_cisv(filepath, parallel=False, fast=False)
    if err:
        print(f"  Skipped: {err}")

    # Run parallel cisv benchmark if requested or if --only-cisv
    if args.parallel or args.only_cisv:
        results['cisv-parallel'], err = benchmark_cisv(filepath, parallel=True, fast=False)
        if err:
            print(f"  Skipped: {err}")

    # Run fast cisv benchmark if requested or if --only-cisv
    if args.fast or args.only_cisv:
        results['cisv-fast'], err = benchmark_cisv(filepath, parallel=False, fast=True, benchmark=False)
        if err:
            print(f"  Skipped: {err}")

    # Run benchmark mode if requested or if --only-cisv
    if args.benchmark or args.only_cisv:
        results['cisv-bench'], err = benchmark_cisv(filepath, parallel=False, fast=False, benchmark=True)
        if err:
            print(f"  Skipped: {err}")

    # Run other benchmarks unless --only-cisv
    if not args.only_cisv:
        results['polars'], err = benchmark_polars(filepath)
        if err:
            print(f"  Skipped: {err}")

        results['pyarrow'], err = benchmark_pyarrow(filepath)
        if err:
            print(f"  Skipped: {err}")

        # Note: datatable crashes on Python 3.13, skip for now
        # results['datatable'], err = benchmark_datatable(filepath)
        # if err:
        #     print(f"  Skipped: {err}")

        results['duckdb'], err = benchmark_duckdb(filepath)
        if err:
            print(f"  Skipped: {err}")

        results['pandas'], err = benchmark_pandas(filepath)
        if err:
            print(f"  Skipped: {err}")

        if not args.skip_stdlib and args.rows <= 1_000_000:
            results['stdlib'], err = benchmark_stdlib(filepath)
            if err:
                print(f"  Skipped: {err}")

    # Print results
    print(f"\n{'='*60}")
    print("RESULTS")
    print(f"{'='*60}")
    print(f"{'Library':<16} {'Parse Time':>12} {'Throughput':>14} {'Rows':>12}")
    print(f"{'-'*60}")

    for name, result in sorted(results.items(), key=lambda x: x[1]['parse_time'] if x[1] else float('inf')):
        if result:
            throughput = format_throughput(file_size, result['parse_time'])
            print(f"{name:<16} {result['parse_time']:>10.3f}s {throughput:>14} {result['parse_rows']:>12,}")

    # cisv count_rows benchmark
    if results.get('cisv') and results['cisv'].get('count_time'):
        count_throughput = format_throughput(file_size, results['cisv']['count_time'])
        print(f"\ncisv count_rows: {results['cisv']['count_time']:.4f}s ({count_throughput})")

    # Cleanup
    if not args.file:
        os.unlink(filepath)
        print(f"\nCleaned up temporary file")


if __name__ == '__main__':
    main()
