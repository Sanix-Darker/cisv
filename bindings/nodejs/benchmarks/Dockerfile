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
RUN npm install --no-save papaparse csv-parse fast-csv csv-parser d3-dsv udsv neat-csv benchmark

# Generate test data in the benchmarks directory
WORKDIR /benchmark/cisv/bindings/nodejs/benchmarks
RUN node -e "const fs = require('fs'); const rows = 100000; let csv = 'id,name,email,value,date\\n'; for(let i=0; i<rows; i++) csv += i+',User_'+i+',user'+i+'@test.com,'+(i*1.23).toFixed(2)+',2024-01-01\\n'; fs.writeFileSync('data.csv', csv);"

# Run benchmark from the benchmarks directory (so ../cisv path works)
WORKDIR /benchmark/cisv/bindings/nodejs/benchmarks
ENTRYPOINT ["node", "benchmark.js", "data.csv"]
