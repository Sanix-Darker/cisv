# CISV Core Library Makefile
CC ?= gcc
AR ?= ar

# Platform and architecture detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# SECURITY: Binary hardening flags
# -fstack-protector-strong: Protect against stack buffer overflows
# -D_FORTIFY_SOURCE=2: Detect buffer overflows in string/memory functions
# -fstack-check: Check for stack overflow at runtime
SECURITY_CFLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2

# Base CFLAGS (architecture-independent)
BASE_CFLAGS = -O3 -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto \
              -ffast-math -funroll-loops -fPIC $(SECURITY_CFLAGS)

# Architecture-specific SIMD flags
ifeq ($(UNAME_M),x86_64)
    # x86_64: Use AVX2 SIMD
    ARCH_CFLAGS = -march=native -mavx2 -mtune=native
else ifeq ($(UNAME_M),amd64)
    # amd64 (same as x86_64 on some systems)
    ARCH_CFLAGS = -march=native -mavx2 -mtune=native
else ifeq ($(UNAME_M),arm64)
    # ARM64 (Apple Silicon, etc.): Use NEON SIMD
    ARCH_CFLAGS = -mcpu=native
else ifeq ($(UNAME_M),aarch64)
    # aarch64 (same as arm64 on Linux)
    ARCH_CFLAGS = -march=native
else
    # Fallback for other architectures
    ARCH_CFLAGS = -march=native
endif

CFLAGS ?= $(BASE_CFLAGS) $(ARCH_CFLAGS)
CFLAGS_DEBUG = -Wall -Wextra -g -O0 -fsanitize=address -fsanitize=undefined \
               -fno-omit-frame-pointer -fPIC

# Platform-specific linker flags
ifeq ($(UNAME_S),Linux)
    # Linux: Full RELRO for GOT protection
    SECURITY_LDFLAGS = -Wl,-z,relro,-z,now
    CFLAGS += -D_GNU_SOURCE -fstack-check
else ifeq ($(UNAME_S),Darwin)
    # macOS: No RELRO equivalent, but use other hardening
    SECURITY_LDFLAGS =
    CFLAGS += -D__APPLE__
else
    SECURITY_LDFLAGS =
endif

LDFLAGS ?= -flto $(SECURITY_LDFLAGS)

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Source files
SRCS = $(SRC_DIR)/parser.c $(SRC_DIR)/writer.c $(SRC_DIR)/transformer.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
OBJS_DEBUG = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.debug.o,$(SRCS))

# Library outputs
STATIC_LIB = $(BUILD_DIR)/libcisv.a
ifeq ($(UNAME_S),Darwin)
    SHARED_LIB = $(BUILD_DIR)/libcisv.dylib
else
    SHARED_LIB = $(BUILD_DIR)/libcisv.so
endif

# Targets
.PHONY: all static shared clean test test-debug install

all: static shared

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Static library
static: $(STATIC_LIB)

$(STATIC_LIB): $(OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

# Shared library
shared: $(SHARED_LIB)

$(SHARED_LIB): $(OBJS) | $(BUILD_DIR)
ifeq ($(UNAME_S),Darwin)
	$(CC) -dynamiclib -o $@ $^ $(LDFLAGS)
else
	$(CC) -shared -o $@ $^ $(LDFLAGS)
endif

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(BUILD_DIR)/%.debug.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS_DEBUG) -I$(INC_DIR) -c -o $@ $<

# Tests
TEST_SRCS = $(TEST_DIR)/test_core.c
TEST_BIN = $(BUILD_DIR)/test_core

test: $(TEST_BIN)
	@echo "Running core tests..."
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TEST_SRCS) $(STATIC_LIB) -lm -lpthread

test-debug: $(OBJS_DEBUG)
	$(CC) $(CFLAGS_DEBUG) -I$(INC_DIR) -o $(BUILD_DIR)/test_core_debug \
		$(TEST_SRCS) $(OBJS_DEBUG) -lm -lpthread
	@echo "Running debug tests..."
	@./$(BUILD_DIR)/test_core_debug

# Installation
PREFIX ?= /usr/local

install: static shared
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/cisv
	install -m 644 $(STATIC_LIB) $(PREFIX)/lib/
	install -m 755 $(SHARED_LIB) $(PREFIX)/lib/
	install -m 644 $(INC_DIR)/cisv/*.h $(PREFIX)/include/cisv/
ifeq ($(UNAME_S),Linux)
	ldconfig || true
endif

uninstall:
	rm -f $(PREFIX)/lib/libcisv.a
	rm -f $(PREFIX)/lib/libcisv.so
	rm -f $(PREFIX)/lib/libcisv.dylib
	rm -rf $(PREFIX)/include/cisv

clean:
	rm -rf $(BUILD_DIR)
