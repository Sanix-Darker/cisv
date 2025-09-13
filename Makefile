CC ?= gcc
CFLAGS ?= -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops
LDFLAGS ?= -flto -s
NODE_GYP ?= node-gyp

# CLI binary name
CLI_BIN = cisv_bin
# Source files for CLI
CLI_SRCS = cisv/cisv_parser.c cisv/cisv_writer.c cisv/cisv_transformer.c
CLI_OBJ = $(CLI_SRCS:.c=.o)

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

# Compile with CISV_CLI defined
cisv/cisv_%.o: cisv/cisv_%.c
	$(CC) $(CFLAGS) -DCISV_CLI -c -o $@ $<

# Install CLI tool
install-cli: cli
	install -m 755 $(CLI_BIN) /usr/local/bin/$(CLI_BIN)

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
	rm -rf $(CLI_BIN) $(CLI_OBJ)
	rm -rf cisv/cisv_cli.o  # Clean old object file if exists

test: build
	npm test

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

.PHONY: all build cli clean test benchmark benchmark-cli benchmark-writer benchmark-all perf coverage package install-cli install-benchmark-deps test-writer
