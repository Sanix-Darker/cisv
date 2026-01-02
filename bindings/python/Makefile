# Python binding Makefile

.PHONY: all build install test clean dev

all: build

build:
	@echo "Building core library..."
	$(MAKE) -C ../../core shared
	@echo "Python binding ready (uses ctypes, no compilation needed)"

install: build
	pip install .

dev: build
	pip install -e ".[dev]"

test: build
	python -m pytest tests/ -v

clean:
	rm -rf build/ dist/ *.egg-info/
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
