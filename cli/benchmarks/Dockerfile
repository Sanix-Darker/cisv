FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Base dependencies
RUN apt-get update && apt-get install -y \
    build-essential curl wget bc time python3 python3-pip \
    miller datamash \
    && rm -rf /var/lib/apt/lists/*

# Install csvkit (Python-based CSV toolkit)
RUN pip3 install --no-cache-dir csvkit

# Install Rust and Rust-based tools (xsv, qsv, frawk)
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN cargo install xsv qsv frawk

# Install csvtk (Go-based, prebuilt binary)
RUN wget -qO- https://github.com/shenwei356/csvtk/releases/latest/download/csvtk_linux_amd64.tar.gz \
    | tar xz -C /usr/local/bin/

# Install tsv-utils (D-based, prebuilt binary)
RUN wget -qO- https://github.com/eBay/tsv-utils/releases/download/v2.1.1/tsv-utils-v2.1.1_linux-x86_64_ldc2.tar.gz \
    | tar xz --strip-components=2 -C /usr/local/bin/

# Install goawk
RUN wget -qO /usr/local/bin/goawk https://github.com/benhoyt/goawk/releases/latest/download/goawk_linux_amd64 \
    && chmod +x /usr/local/bin/goawk

WORKDIR /benchmark
COPY . /benchmark/cisv/

# Build cisv CLI
WORKDIR /benchmark/cisv
RUN make clean || true && make core cli

COPY cli/benchmarks/run_benchmark.sh /benchmark/
RUN chmod +x /benchmark/run_benchmark.sh

WORKDIR /benchmark
ENTRYPOINT ["/benchmark/run_benchmark.sh"]
CMD ["--rows=100000", "--cols=7", "--iterations=5"]
