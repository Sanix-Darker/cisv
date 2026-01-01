# CISV Core Library Makefile
CC ?= gcc
AR ?= ar
CFLAGS ?= -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer \
          -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops -fPIC
CFLAGS_DEBUG = -Wall -Wextra -g -O0 -fsanitize=address -fsanitize=undefined \
               -fno-omit-frame-pointer -fPIC
LDFLAGS ?= -flto

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
SHARED_LIB = $(BUILD_DIR)/libcisv.so

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__
    SHARED_LIB = $(BUILD_DIR)/libcisv.dylib
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
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TEST_SRCS) -L$(BUILD_DIR) -lcisv -lm -lpthread

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
