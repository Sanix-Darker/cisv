#!/usr/bin/env python3

"""
Benchmark comparison: cisv vs pandas vs polars for CSV parsing

# Install dependencies
pip install --upgrade --no-cache-dir cisv pandas polars

# Run benchmark with 1 million rows × 10 columns (~100MB file)
python scripts/benchmark_python.py --rows 1000000 --cols 10

# Run with 10 million rows × 20 columns (~2GB file)
python scripts/benchmark_python.py --rows 10000000 --cols 20

# Use an existing CSV file
python scripts/benchmark_python.py --file /path/to/large.csv
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


def benchmark_cisv(filepath: str) -> tuple:
    """Benchmark cisv parser."""
    try:
        import cisv
    except ImportError:
        return None, "cisv not installed"

    print("Benchmarking cisv...")

    # Warm up
    cisv.count_rows(filepath)

    # Count rows (fast path)
    start = time.perf_counter()
    row_count = cisv.count_rows(filepath)
    count_time = time.perf_counter() - start

    # Full parse
    start = time.perf_counter()
    rows = cisv.parse_file(filepath)
    parse_time = time.perf_counter() - start

    return {
        'count_time': count_time,
        'count_rows': row_count,
        'parse_time': parse_time,
        'parse_rows': len(rows),
        'parse_cols': len(rows[0]) if rows else 0,
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

    # Run benchmarks
    results['cisv'], err = benchmark_cisv(filepath)
    if err:
        print(f"  Skipped: {err}")

    results['polars'], err = benchmark_polars(filepath)
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
    print(f"{'Library':<12} {'Parse Time':>12} {'Throughput':>14} {'Rows':>12}")
    print(f"{'-'*60}")

    for name, result in sorted(results.items(), key=lambda x: x[1]['parse_time'] if x[1] else float('inf')):
        if result:
            throughput = format_throughput(file_size, result['parse_time'])
            print(f"{name:<12} {result['parse_time']:>10.3f}s {throughput:>14} {result['parse_rows']:>12,}")

    # cisv count_rows benchmark
    if results.get('cisv') and results['cisv'].get('count_time'):
        print(f"\ncisv count_rows (fast): {results['cisv']['count_time']:.3f}s")

    # Cleanup
    if not args.file:
        os.unlink(filepath)
        print(f"\nCleaned up temporary file")


if __name__ == '__main__':
    main()
