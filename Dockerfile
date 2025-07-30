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
    cppcheck \
    clang \
    llvm \
    libc6-dev \
    nodejs \
    npm \
    miller \
    && rm -rf /var/lib/apt/lists/*

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install Rust CSV tools
RUN cargo install xsv qsv

# Install Python CSV tools
RUN pip3 install --upgrade pip && \
    pip3 install 'babel>=2.9.0' 'csvkit>=1.1.0' pandas

# Install node-gyp globally
RUN npm install -g node-gyp node-addon-api

# Create benchmark user and directory
RUN useradd -m -s /bin/bash benchmark
WORKDIR /home/benchmark

# Copy the project
COPY --chown=benchmark:benchmark . /home/benchmark/cisv

# Build rust-csv benchmark tool
WORKDIR /home/benchmark/cisv
RUN make install-benchmark-deps || true

# Build cisv
RUN make clean && make cli

# Create benchmark script
RUN cat > /home/benchmark/run_benchmarks.sh << 'EOF'
#!/bin/bash
set -e

echo "=== CISV Benchmark Suite ==="
echo "Date: $(date)"
echo "CPU: $(lscpu | grep 'Model name' | cut -d: -f2 | xargs)"
echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
echo "Kernel: $(uname -r)"
echo ""

cd /home/benchmark/cisv

# Run memory tests
echo "=== Memory Leak Tests ==="
valgrind --leak-check=full --show-leak-kinds=all ./cisv --version
echo ""

# Small benchmark for memory check
echo "id,name,value" > test.csv
echo "1,test,100" >> test.csv
echo "2,demo,200" >> test.csv
valgrind --leak-check=full ./cisv -c test.csv
echo ""

# Run full benchmarks
echo "=== Performance Benchmarks ==="
chmod +x benchmark_cli.sh
./benchmark_cli.sh

# Run Node.js benchmarks if available
if [ -f "benchmark/benchmark.js" ]; then
    echo ""
    echo "=== Node.js Benchmarks ==="
    npm install || true
    npm run benchmark || true
fi
EOF

RUN chmod +x /home/benchmark/run_benchmarks.sh

# Switch to benchmark user
USER benchmark
WORKDIR /home/benchmark/cisv

# Default command
CMD ["/home/benchmark/run_benchmarks.sh"]
