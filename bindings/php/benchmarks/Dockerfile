FROM php:8.2-cli

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential git autoconf \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /benchmark
COPY . /benchmark/cisv/

# Build core
WORKDIR /benchmark/cisv
RUN make clean || true && make core

# Build PHP extension
WORKDIR /benchmark/cisv/bindings/php
RUN phpize && ./configure && make

COPY bindings/php/benchmarks/benchmark.php /benchmark/

WORKDIR /benchmark
ENTRYPOINT ["php", "-d", "extension=/benchmark/cisv/bindings/php/modules/cisv.so", "/benchmark/benchmark.php"]
