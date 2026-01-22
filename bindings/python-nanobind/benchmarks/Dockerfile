FROM python:3.12-slim

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    && rm -rf /var/lib/apt/lists/*

# Install comparison libraries
RUN pip install --no-cache-dir \
    polars pyarrow pandas duckdb numpy

# Copy project
WORKDIR /benchmark
COPY . /benchmark/cisv/

# Build and install cisv
WORKDIR /benchmark/cisv
RUN make clean || true && make core
WORKDIR /benchmark/cisv/bindings/python-nanobind
RUN pip install --no-cache-dir .

# Copy benchmark script
COPY bindings/python-nanobind/benchmarks/benchmark.py /benchmark/

WORKDIR /benchmark

# Default: 1M rows x 10 cols
ENTRYPOINT ["python", "/benchmark/benchmark.py", "--rows", "1000000", "--cols", "10"]
