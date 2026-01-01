# CISV CLI Makefile
CC ?= gcc
CFLAGS ?= -O3 -march=native -mavx2 -mtune=native -pipe -fomit-frame-pointer \
          -Wall -Wextra -std=c11 -flto -ffast-math -funroll-loops
LDFLAGS ?= -flto -s

# Directories
SRC_DIR = src
BUILD_DIR = build
CORE_DIR = ../core

# Source files
SRCS = $(SRC_DIR)/main.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Binary
CLI_BIN = $(BUILD_DIR)/cisv

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
    LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__
endif

# Targets
.PHONY: all clean install uninstall

all: $(CLI_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure core library is built first
$(CORE_DIR)/build/libcisv.a:
	$(MAKE) -C $(CORE_DIR) static

$(CLI_BIN): $(OBJS) $(CORE_DIR)/build/libcisv.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(CORE_DIR)/build -lcisv $(LDFLAGS) -lm -lpthread

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(CORE_DIR)/include -c -o $@ $<

# Installation
PREFIX ?= /usr/local

install: $(CLI_BIN)
	install -d $(PREFIX)/bin
	install -m 755 $(CLI_BIN) $(PREFIX)/bin/cisv

uninstall:
	rm -f $(PREFIX)/bin/cisv

clean:
	rm -rf $(BUILD_DIR)

# Test CLI
test: $(CLI_BIN)
	@echo "Testing CLI..."
	@echo "a,b,c" > /tmp/test.csv
	@echo "1,2,3" >> /tmp/test.csv
	@$(CLI_BIN) -c /tmp/test.csv
	@$(CLI_BIN) /tmp/test.csv
	@rm /tmp/test.csv
	@echo "CLI tests passed"
