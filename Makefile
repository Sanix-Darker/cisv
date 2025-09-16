CC ?= gcc
CFLAGS ?= -O3 -march=native -pipe -Wall -Wextra -std=c11 -flto -ffast-math
LDFLAGS ?= -flto -s
NODE_GYP ?= node-gyp
PYTHON_CONFIG ?= python3-config
PHP_CONFIG ?= php-config

# Directories
SRC_DIR = src/core
INCLUDE_DIR = include
BINDINGS_DIR = bindings
BIN_DIR = bin
BUILD_DIR = build

# Core source files
CORE_SRCS = $(SRC_DIR)/cisv_parser.c $(SRC_DIR)/cisv_transformer.c $(SRC_DIR)/cisv_writer.c
CORE_OBJS = $(CORE_SRCS:.c=.o)

# SIMD-specific files
SIMD_SRCS = $(SRC_DIR)/cisv_parser_avx512.c $(SRC_DIR)/cisv_parser_avx2.c \
            $(SRC_DIR)/cisv_parser_neon.c $(SRC_DIR)/cisv_parser_scalar.c
SIMD_OBJS = $(SIMD_SRCS:.c=.o)

# CLI binary
CLI_BIN = $(BIN_DIR)/cisv
CLI_SRC = src/cli/main.c
CLI_OBJ = $(CLI_SRC:.c=.o)

# Detect SIMD support
ifeq ($(shell grep -m1 -o avx512f /proc/cpuinfo),avx512f)
    CFLAGS += -mavx512f -DCISV_HAVE_AVX512
endif
ifeq ($(shell grep -m1 -o avx2 /proc/cpuinfo),avx2)
    CFLAGS += -mavx2 -DCISV_HAVE_AVX2
endif
ifeq ($(shell grep -m1 -o neon /proc/cpuinfo),neon)
    CFLAGS += -mfpu=neon -DCISV_HAVE_NEON
endif

# Build targets
all: core cli node python php

core: $(CORE_OBJS) $(SIMD_OBJS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c -o $@ $<

cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_OBJ) $(CORE_OBJS) $(SIMD_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLI_OBJ): $(CLI_SRC)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c -o $@ $<

node:
	cd $(BINDINGS_DIR)/node && npm install
	$(NODE_GYP) configure build --directory $(BINDINGS_DIR)/node CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

python:
	cd $(BINDINGS_DIR)/python && \
	CFLAGS="$(CFLAGS) $$($(PYTHON_CONFIG) --cflags)" \
	LDFLAGS="$(LDFLAGS) $$($(PYTHON_CONFIG) --ldflags)" \
	python3 setup.py build_ext --inplace

php:
	cd $(BINDINGS_DIR)/php && \
	phpize && \
	./configure CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" && \
	make

install:
	npm install -g node-gyp
	npm install -g node-addon-api

install-cli: cli
	install -m 755 $(CLI_BIN) /usr/local/bin/cisv

install-python: python
	cd $(BINDINGS_DIR)/python && python3 setup.py install

install-php: php
	cd $(BINDINGS_DIR)/php && make install

clean:
	rm -f $(CORE_OBJS) $(SIMD_OBJS) $(CLI_OBJ) $(CLI_BIN)
	rm -rf $(BINDINGS_DIR)/node/build
	rm -rf $(BINDINGS_DIR)/python/build
	rm -rf $(BINDINGS_DIR)/php/modules
	rm -f $(BINDINGS_DIR)/python/*.so
	$(NODE_GYP) clean --directory $(BINDINGS_DIR)/node)
	cd $(BINDINGS_DIR)/php && phpize --clean

test: node python php
	cd tests/node && npm test
	cd tests/python && python3 -m pytest
	cd tests/php && phpunit test_cisv.php

benchmark: node python php
	node benchmark/benchmark.js
	python3 benchmark/benchmark.py
	php benchmark/benchmark.php

.PHONY: all core cli node python php install install-cli install-python install-php clean test benchmark
