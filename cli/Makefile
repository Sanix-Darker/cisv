# CISV CLI Makefile
CC ?= gcc

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
PORTABLE ?= 0

# SECURITY: Binary hardening flags for executables
# -fstack-protector-strong: Protect against stack buffer overflows
# -D_FORTIFY_SOURCE=2: Detect buffer overflows in string/memory functions
# -fPIE / -pie: Position Independent Executable for ASLR
# -Wl,-z,relro,-z,now: Full RELRO - make GOT read-only after relocation
# -fstack-check: Check for stack overflow at runtime
SECURITY_CFLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fstack-check -fPIE
SECURITY_LDFLAGS =

BASE_CFLAGS = -O3 -pipe -fomit-frame-pointer -Wall -Wextra -std=c11 -flto \
              -ffast-math -funroll-loops $(SECURITY_CFLAGS)

ifeq ($(PORTABLE),1)
    ifeq ($(UNAME_M),x86_64)
        ARCH_CFLAGS = -msse4.2
    else ifeq ($(UNAME_M),amd64)
        ARCH_CFLAGS = -msse4.2
    else ifeq ($(UNAME_M),arm64)
        ARCH_CFLAGS = -march=armv8-a
    else ifeq ($(UNAME_M),aarch64)
        ARCH_CFLAGS = -march=armv8-a
    else
        ARCH_CFLAGS =
    endif
else
    ifeq ($(UNAME_M),x86_64)
        ARCH_CFLAGS = -march=native -mavx2 -mtune=native
    else ifeq ($(UNAME_M),amd64)
        ARCH_CFLAGS = -march=native -mavx2 -mtune=native
    else ifeq ($(UNAME_M),arm64)
        ARCH_CFLAGS = -mcpu=native
    else ifeq ($(UNAME_M),aarch64)
        ARCH_CFLAGS = -march=native
    else
        ARCH_CFLAGS = -march=native
    endif
endif

CFLAGS ?= $(BASE_CFLAGS) $(ARCH_CFLAGS)
LDFLAGS ?= -flto -s $(SECURITY_LDFLAGS)

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
ifeq ($(UNAME_S),Linux)
    SECURITY_LDFLAGS = -Wl,-z,relro,-z,now -pie
    CFLAGS += -D_GNU_SOURCE
    LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
    SECURITY_LDFLAGS =
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
