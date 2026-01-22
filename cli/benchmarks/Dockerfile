FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential curl bc time miller python3 \
    && rm -rf /var/lib/apt/lists/*

# Install Rust and xsv
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN cargo install xsv

WORKDIR /benchmark
COPY . /benchmark/cisv/

# Build cisv CLI
WORKDIR /benchmark/cisv
RUN make clean || true && make core cli

COPY cli/benchmarks/run_benchmark.sh /benchmark/
RUN chmod +x /benchmark/run_benchmark.sh

WORKDIR /benchmark
ENTRYPOINT ["/benchmark/run_benchmark.sh"]
