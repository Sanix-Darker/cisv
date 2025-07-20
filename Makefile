CC ?= gcc
CFLAGS ?= -O3 -march=native -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto
LDFLAGS ?= -flto
NODE_GYP ?= node-gyp

# Build targets
all: build

install:
	npm install -g node-gyp
	npm install -g node-addon-api

build:
	npm install
	$(NODE_GYP) configure build CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

debug:
	$(NODE_GYP) configure build --debug

clean:
	$(NODE_GYP) clean
	rm -rf build

test: build
	npm test

benchmark: build
	node benchmark/benchmark.js $(SAMPLE)

perf: build
	node test/performance.test.js

coverage:
	$(NODE_GYP) configure --coverage
	$(MAKE) test

package: clean build test
	npm pack

.PHONY: all build clean test benchmark perf coverage package
