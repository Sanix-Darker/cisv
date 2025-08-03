# Dockerfile for CISV benchmarking with CPU/RAM isolation
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    curl \
    wget \
    python3 \
    python3-pip \
    bc \
    time \
    valgrind \
    clang \
    libc6-dev \
    miller \
    ruby \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js 20.x
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install Rust CSV tools
RUN cargo install xsv qsv

# Install Python CSV tools
RUN pip3 install --upgrade pip && \
    pip3 install 'babel>=2.9.0' 'csvkit>=1.1.0' pandas

# Install Node.js CSV tools (with legacy peer deps for compatibility)
RUN npm install -g node-gyp node-addon-api fast-csv --legacy-peer-deps

# Create benchmark user and directory
RUN useradd -m -s /bin/bash benchmark
WORKDIR /home/benchmark

# Copy the project
COPY --chown=benchmark:benchmark . /home/benchmark/cisv

# Build cisv and dependencies
WORKDIR /home/benchmark/cisv
# Skip install-benchmark-deps since we already installed dependencies
RUN make clean && cargo install qsv && make install-benchmark-deps && make cli

# Create main benchmark runner script
COPY ./benchmark_cli_writer.sh ./benchmark_cli_reader.sh ./run_benchmarks.sh /home/benchmark/
RUN chmod +x /home/benchmark/run_benchmarks.sh

# Switch to benchmark user
USER benchmark
WORKDIR /home/benchmark/cisv

# Default command runs all benchmarks
CMD ["/home/benchmark/run_benchmarks.sh", "all"]
