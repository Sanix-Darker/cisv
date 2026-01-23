FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Base dependencies
RUN apt-get update && apt-get install -y \
    build-essential curl wget bc time python3 python3-pip unzip \
    miller datamash \
    && rm -rf /var/lib/apt/lists/*

# Install csvkit (Python-based CSV toolkit)
RUN pip3 install --no-cache-dir csvkit

# Install Rust and xsv (from source - fast to compile)
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN cargo install xsv

# Install qsv (prebuilt musl binary - statically linked, no external deps)
RUN wget -qO /tmp/qsv.zip https://github.com/dathere/qsv/releases/download/14.0.0/qsv-14.0.0-x86_64-unknown-linux-musl.zip \
    && unzip -q /tmp/qsv.zip -d /tmp/qsv \
    && mv /tmp/qsv/qsv /usr/local/bin/ \
    && chmod +x /usr/local/bin/qsv \
    && rm -rf /tmp/qsv /tmp/qsv.zip

# Install frawk (prebuilt binary - no-llvm variant)
RUN wget -qO /tmp/frawk.tar.gz https://github.com/ezrosent/frawk/releases/download/v0.4.1/linux-x86-musl-no-llvm.tar.gz \
    && tar xzf /tmp/frawk.tar.gz -C /usr/local/bin/ \
    && chmod +x /usr/local/bin/frawk \
    && rm /tmp/frawk.tar.gz

# Install csvtk (Go-based, prebuilt binary)
RUN wget -qO- https://github.com/shenwei356/csvtk/releases/latest/download/csvtk_linux_amd64.tar.gz \
    | tar xz -C /usr/local/bin/

# Install tsv-utils (D-based, prebuilt binary)
RUN wget -qO- https://github.com/eBay/tsv-utils/releases/download/v2.1.1/tsv-utils-v2.1.1_linux-x86_64_ldc2.tar.gz \
    | tar xz --strip-components=2 -C /usr/local/bin/

# Install goawk
RUN wget -qO- https://github.com/benhoyt/goawk/releases/download/v1.31.0/goawk_v1.31.0_linux_amd64.tar.gz \
    | tar xz -C /usr/local/bin/

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
