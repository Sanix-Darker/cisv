# Compiler and flags
CC ?= gcc
CFLAGS ?= -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops
CFLAGS_DEBUG = -Wall -Wextra -g -O0 -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LDFLAGS ?= -flto -s
LDFLAGS_TEST = -lm -lpthread
NODE_GYP ?= node-gyp

# Feature detection
UNAME_S := $(shell uname -s)
ARCH := $(shell uname -m)

# Platform-specific settings
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
    LDFLAGS_TEST += -lrt
endif

ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__
endif

# CLI binary name
CLI_BIN = cisv_bin
# Source files for CLI
CLI_SRCS = cisv/cisv_parser.c cisv/cisv_writer.c cisv/cisv_transformer.c
CLI_OBJ = $(CLI_SRCS:.c=.cli.o)

# Test files
TEST_SOURCES = tests/test_native.c
TEST_BINARY = tests/test_native

# Library object files for testing (WITHOUT -DCISV_CLI)
TEST_LIB_SRCS = cisv/cisv_parser.c cisv/cisv_writer.c cisv/cisv_transformer.c
TEST_LIB_OBJ = $(TEST_LIB_SRCS:.c=.test.o)
TEST_LIB_OBJ_DEBUG = $(TEST_LIB_SRCS:.c=.test.debug.o)

# Build targets
all: build cli

install:
	npm install -g node-gyp
	npm install -g node-addon-api

build:
	npm install
	$(NODE_GYP) configure build CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

# Build CLI tool
cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile CLI objects WITH CISV_CLI defined
cisv/%.cli.o: cisv/%.c
	$(CC) $(CFLAGS) -DCISV_CLI -c -o $@ $<

# Compile test objects WITHOUT CISV_CLI defined
cisv/%.test.o: cisv/%.c
	$(CC) $(CFLAGS) -Icisv -c -o $@ $<

# Debug object files for testing (WITHOUT CISV_CLI)
cisv/%.test.debug.o: cisv/%.c
	$(CC) $(CFLAGS_DEBUG) -Icisv -c -o $@ $<

# Install CLI tool
install-cli: cli
	install -m 755 $(CLI_BIN) /usr/local/bin/$(CLI_BIN)

# ==================== TEST TARGETS ====================

# Build test binary
tests: $(TEST_BINARY)

$(TEST_BINARY): $(TEST_SOURCES) $(TEST_LIB_OBJ)
	$(CC) $(CFLAGS) -Icisv -o $@ $(TEST_SOURCES) $(TEST_LIB_OBJ) $(LDFLAGS_TEST)

# Run C tests
test-c: tests
	@echo "Running CISV C test suite..."
	@./$(TEST_BINARY)

# Run Node.js tests (existing)
test: build
	npm test

# Run all tests
test-all: test-c test

# Run tests with valgrind
test-valgrind: tests
	@echo "Running tests with Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		./$(TEST_BINARY)

# Run tests with debug build
test-debug: $(TEST_SOURCES) $(TEST_LIB_OBJ_DEBUG)
	@echo "Building debug test binary..."
	$(CC) $(CFLAGS_DEBUG) -Icisv -o $(TEST_BINARY).debug $(TEST_SOURCES) $(TEST_LIB_OBJ_DEBUG) $(LDFLAGS_TEST)
	@echo "Running debug tests..."
	@./$(TEST_BINARY).debug

# Run tests with coverage
test-coverage:
	@echo "Building with coverage..."
	$(CC) $(CFLAGS) --coverage -Icisv -o $(TEST_BINARY).cov $(TEST_SOURCES) $(TEST_LIB_SRCS) $(LDFLAGS_TEST)
	@echo "Running coverage tests..."
	@./$(TEST_BINARY).cov
	@echo "Generating coverage report..."
	@gcov $(TEST_LIB_SRCS) $(TEST_SOURCES)

# Memory check
memcheck: tests
	@echo "Running memory checks..."
	@valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
		--num-callers=20 --track-origins=yes ./$(TEST_BINARY)

# Generate test data
test-data:
	@echo "Generating test data files..."
	@mkdir -p testdata
	@echo "name,age,city" > testdata/simple.csv
	@echo "John,25,NYC" >> testdata/simple.csv
	@echo "Jane,30,LA" >> testdata/simple.csv
	@echo '"quoted","with,comma","with""quote"' > testdata/quoted.csv
	@echo 'name;age;city' > testdata/semicolon.csv
	@echo 'John;25;NYC' >> testdata/semicolon.csv
	@echo "# Comment line" > testdata/comments.csv
	@echo "name,age" >> testdata/comments.csv
	@echo "# Another comment" >> testdata/comments.csv
	@echo "John,25" >> testdata/comments.csv
	@echo "Generated test data in testdata/"

# Install benchmark dependencies
install-benchmark-deps:
	@echo "Installing benchmark dependencies..."
	@# Install Rust if not present
	@if ! command -v cargo > /dev/null; then \
		echo "Installing Rust..."; \
		curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y; \
		. $$HOME/.cargo/env; \
	fi
	@# Install xsv
	@if ! command -v xsv > /dev/null; then \
		echo "Installing xsv..."; \
		cargo install xsv; \
	fi
	@# Install qsv (faster fork of xsv)
	@if ! command -v qsv > /dev/null; then \
		echo "Installing qsv..."; \
		cargo install qsv; \
	fi
	@# Build rust-csv example
	@echo "Building rust-csv benchmark tool..."
	@mkdir -p benchmark/rust-csv-bench
	@cd benchmark/rust-csv-bench && \
		if [ ! -f Cargo.toml ]; then \
			cargo init --name csv-bench; \
			echo 'csv = "1.3"' >> Cargo.toml; \
		fi && \
		echo 'use std::env;\nuse std::error::Error;\nuse csv::ReaderBuilder;\n\nfn main() -> Result<(), Box<dyn Error>> {\n    let args: Vec<String> = env::args().collect();\n    if args.len() < 2 { eprintln!("Usage: {} <file>", args[0]); std::process::exit(1); }\n    let mut rdr = ReaderBuilder::new().has_headers(true).from_path(&args[1])?;\n    let count = rdr.records().count();\n    println!("{}", count);\n    Ok(())\n}' > ./main.rs && \
		cargo build --release
	@# Fix csvkit Python 3.12 compatibility issue and install
	@if command -v pip3 > /dev/null; then \
		echo "Installing/fixing csvkit..."; \
		pip3 install --upgrade pip; \
		pip3 uninstall -y csvkit babel || true; \
		pip3 install 'babel>=2.9.0' 'csvkit>=1.1.0'; \
	fi

debug:
	$(NODE_GYP) configure build --debug

clean:
	$(NODE_GYP) clean
	rm -rf build
	rm -rf $(CLI_BIN) $(CLI_OBJ) $(TEST_LIB_OBJ) $(TEST_LIB_OBJ_DEBUG)
	rm -rf cisv/*.o  # Clean all object files
	rm -f $(TEST_BINARY) $(TEST_BINARY).debug $(TEST_BINARY).cov
	rm -f *.gcno *.gcda *.gcov coverage.info
	rm -rf testdata

# Benchmark comparing cisv CLI with other tools
benchmark-cli: cli
	@echo "=== Benchmarking CSV CLI tools ==="
	@echo "Preparing test file..."
	@python3 -c "import csv; w=csv.writer(open('bench_test.csv','w')); [w.writerow([i,f'name_{i}',f'email_{i}@test.com',f'city_{i}']) for i in range(1000000)]"
	@echo ""
	@echo "--- cisv (this project) ---"
	@bash -c "TIMEFORMAT='real %3R'; time ./$(CLI_BIN) -c bench_test.csv 2>&1"
	@if [ -f benchmark/rust-csv-bench/target/release/csv-bench ]; then \
		echo ""; \
		echo "--- rust-csv (Rust library) ---"; \
		bash -c "TIMEFORMAT='real %3R'; time ./benchmark/rust-csv-bench/target/release/csv-bench bench_test.csv 2>&1"; \
	fi
	@if command -v xsv > /dev/null 2>&1; then \
		echo ""; \
		echo "--- xsv (Rust CLI) ---"; \
		bash -c "TIMEFORMAT='real %3R'; time xsv count bench_test.csv 2>&1"; \
	fi
	@if command -v qsv > /dev/null 2>&1; then \
		echo ""; \
		echo "--- qsv (Rust CLI - faster xsv fork) ---"; \
		bash -c "TIMEFORMAT='real %3R'; time qsv count bench_test.csv 2>&1"; \
	fi
	@if command -v wc > /dev/null 2>&1; then \
		echo ""; \
		echo "--- wc -l (baseline) ---"; \
		bash -c "TIMEFORMAT='real %3R'; time wc -l bench_test.csv 2>&1"; \
	fi
	@if command -v csvstat > /dev/null 2>&1; then \
		echo ""; \
		echo "--- csvkit (Python) ---"; \
		bash -c "TIMEFORMAT='real %3R'; time csvstat --count bench_test.csv 2>&1" || echo "csvkit failed - may need: pip3 install --upgrade babel csvkit"; \
	fi
	@if command -v mlr > /dev/null 2>&1; then \
		echo ""; \
		echo "--- Miller ---"; \
		bash -c "TIMEFORMAT='real %3R'; time mlr --csv count bench_test.csv 2>&1"; \
	fi
	@echo ""
	@echo "File size: " && ls -lh bench_test.csv | awk '{print $$5}'
	@rm -f bench_test.csv

benchmark: build
	node benchmark/benchmark.js $(SAMPLE)

perf: build
	node test/performance.test.js

coverage:
	$(NODE_GYP) configure --coverage
	$(MAKE) test

package: clean build test
	npm pack

# Test writer functionality
test-writer: cli
	chmod +x test_writer.sh
	./test_writer.sh

# Benchmark writer performance
benchmark-writer: cli
	chmod +x benchmark_cli_writer.sh
	./benchmark_cli_writer.sh

# Run all benchmarks
benchmark-all: benchmark-cli benchmark-writer

# Help target
help:
	@echo "CISV Makefile - Main Targets:"
	@echo ""
	@echo "Building:"
	@echo "  make all          - Build Node.js addon and CLI"
	@echo "  make cli          - Build command-line tool"
	@echo "  make build        - Build Node.js addon"
	@echo ""
	@echo "Testing:"
	@echo "  make test         - Run Node.js tests"
	@echo "  make test-c       - Run C test suite"
	@echo "  make test-all     - Run all tests"
	@echo "  make test-debug   - Run tests with debug build"
	@echo "  make test-valgrind - Run tests with Valgrind"
	@echo "  make test-coverage - Run tests with code coverage"
	@echo "  make memcheck     - Check for memory leaks"
	@echo "  make test-data    - Generate test CSV files"
	@echo "  make test-writer  - Test writer functionality"
	@echo ""
	@echo "Benchmarking:"
	@echo "  make benchmark    - Run Node.js benchmark"
	@echo "  make benchmark-cli - Benchmark CLI vs other tools"
	@echo "  make benchmark-writer - Benchmark writer"
	@echo "  make benchmark-all - Run all benchmarks"
	@echo "  make perf         - Run performance tests"
	@echo ""
	@echo "Installation:"
	@echo "  make install      - Install Node dependencies"
	@echo "  make install-cli  - Install CLI to /usr/local/bin"
	@echo "  make install-benchmark-deps - Install benchmark tools"
	@echo ""
	@echo "Other:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make coverage     - Build with coverage"
	@echo "  make package      - Create npm package"
	@echo "  make help         - Show this help"

.PHONY: all build cli clean test benchmark benchmark-cli benchmark-writer benchmark-all perf coverage package install-cli install-benchmark-deps test-writer
.PHONY: tests test-c test-all test-debug test-valgrind test-coverage memcheck test-data help
