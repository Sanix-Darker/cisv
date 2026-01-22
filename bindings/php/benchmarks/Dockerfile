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

# Install composer and league/csv for benchmark comparison
RUN apt-get update && apt-get install -y curl unzip && rm -rf /var/lib/apt/lists/*
RUN curl -sS https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer
RUN mkdir -p /tmp/csv_bench && cd /tmp/csv_bench && \
    echo '{"require": {"league/csv": "^9.0"}}' > composer.json && \
    composer install --no-dev --no-interaction

COPY bindings/php/benchmarks/benchmark.php /benchmark/

WORKDIR /benchmark
ENTRYPOINT ["php", "-d", "extension=/benchmark/cisv/bindings/php/modules/cisv.so", "/benchmark/benchmark.php"]
