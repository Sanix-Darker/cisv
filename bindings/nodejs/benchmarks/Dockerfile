FROM node:20-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential python3 git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /benchmark
COPY . /benchmark/cisv/

# Build core library
WORKDIR /benchmark/cisv
RUN make clean || true && make core

# Build Node.js binding and install benchmark deps
WORKDIR /benchmark/cisv/bindings/nodejs
RUN npm install
RUN npm run build
RUN npm install --no-save papaparse csv-parse fast-csv d3-dsv udsv neat-csv

# Run benchmark from the benchmarks directory
WORKDIR /benchmark/cisv/bindings/nodejs/benchmarks
ENTRYPOINT ["node", "benchmark.js"]
CMD ["--rows=100000", "--cols=5", "--iterations=5"]
