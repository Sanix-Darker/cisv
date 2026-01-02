# CISV - High-Performance CSV Parser
# Root Makefile - Orchestrates all modules

.PHONY: all core cli nodejs python php clean test install help

# Default target
all: core cli

# Build core library
core:
	@echo "Building core library..."
	$(MAKE) -C core all

# Build CLI tool
cli: core
	@echo "Building CLI..."
	$(MAKE) -C cli all

# Build Node.js binding
nodejs:
	@echo "Building Node.js binding..."
	cd bindings/nodejs && npm install

# Build Python binding
python: core
	@echo "Building Python binding..."
	$(MAKE) -C bindings/python build

# Build PHP extension (requires phpize)
php: core
	@echo "Building PHP extension..."
	$(MAKE) -C bindings/php build

# Run all tests
test: test-core test-cli test-nodejs

test-core: core
	@echo "Running core tests..."
	@cd core && make test

test-cli: cli
	@echo "Running CLI tests..."
	@$(MAKE) -C cli test

test-nodejs: nodejs
	@echo "Running Node.js tests..."
	cd bindings/nodejs && npm test

# Clean all build artifacts
clean:
	$(MAKE) -C core clean
	$(MAKE) -C cli clean
	cd bindings/nodejs && rm -rf build node_modules 2>/dev/null || true
	$(MAKE) -C bindings/python clean 2>/dev/null || true
	$(MAKE) -C bindings/php clean 2>/dev/null || true
	rm -rf build_cmake

# Install
install: core cli
	$(MAKE) -C core install
	$(MAKE) -C cli install

# Uninstall
uninstall:
	$(MAKE) -C core uninstall
	$(MAKE) -C cli uninstall

# Benchmarks
benchmark-cli: cli
	@echo "=== CLI Benchmark ==="
	@python3 -c "import csv; w=csv.writer(open('bench_test.csv','w')); [w.writerow([i,f'name_{i}',f'email_{i}@test.com',f'city_{i}']) for i in range(1000000)]"
	@echo "Testing with 1M rows..."
	@LD_LIBRARY_PATH=core/build time cli/build/cisv -c bench_test.csv
	@rm -f bench_test.csv

benchmark-nodejs: nodejs
	@echo "=== Node.js Benchmark ==="
	cd bindings/nodejs && node benchmark.js 2>/dev/null || echo "Run npm install first"

# CMake build (alternative)
cmake-build:
	mkdir -p build_cmake
	cd build_cmake && cmake .. -DCMAKE_BUILD_TYPE=Release
	$(MAKE) -C build_cmake -j4

cmake-test: cmake-build
	cd build_cmake && ctest --output-on-failure

# Help
help:
	@echo "CISV Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build core library and CLI (default)"
	@echo "  core         Build core C library"
	@echo "  cli          Build CLI tool"
	@echo "  nodejs       Build Node.js binding"
	@echo "  python       Build Python binding"
	@echo "  php          Build PHP extension"
	@echo ""
	@echo "  test         Run all tests"
	@echo "  test-core    Run core library tests"
	@echo "  test-cli     Run CLI tests"
	@echo "  test-nodejs  Run Node.js tests"
	@echo ""
	@echo "  clean        Clean all build artifacts"
	@echo "  install      Install library and CLI"
	@echo "  uninstall    Uninstall library and CLI"
	@echo ""
	@echo "  benchmark-cli    Run CLI benchmark"
	@echo "  benchmark-nodejs Run Node.js benchmark"
	@echo ""
	@echo "  cmake-build  Build using CMake"
	@echo "  cmake-test   Test CMake build"
	@echo ""
	@echo "  help         Show this help"
