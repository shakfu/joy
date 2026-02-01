# Makefile for compiled Joy examples
#
# Usage:
#   make simple    - Build simple example
#   make fib       - Build fibonacci example
#   make all       - Build all examples
#   make clean     - Remove generated files

# Joy source directory (adjust if needed)
JOY_DIR := ../..
JOY_BUILD := $(JOY_DIR)/build
JOY_INC := $(JOY_DIR)/include
JOY_GEN := $(JOY_BUILD)/generated

# Compiler settings
CC := gcc
CFLAGS := -O3 -DNOBDW -Wall -Wno-unused-parameter
INCLUDES := -I$(JOY_INC) -I$(JOY_GEN) -I$(JOY_DIR)
LIBS := $(JOY_BUILD)/libjoycore_static.a -lm -lsqlite3

# Python for code generator
PYTHON := python3
JOY2C := $(JOY_DIR)/tools/joy2c.py

# Targets
EXAMPLES := simple fib benchmark

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

# Pattern rule: .joy -> .c
%.c: %.joy $(JOY2C)
	$(PYTHON) $(JOY2C) $< --inline --fold -o $@

# Pattern rule: .c -> executable
%: %.c $(JOY_BUILD)/libjoycore_static.a
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIBS) -o $@

# Individual targets
simple: simple.c
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIBS) -o $@

fib: fib.c
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIBS) -o $@

benchmark: benchmark.c
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIBS) -o $@

# Ensure build directory exists
$(JOY_BUILD)/libjoycore_static.a:
	@echo "Error: Joy library not built. Run 'cmake -B build && cmake --build build' in $(JOY_DIR)"
	@exit 1

clean:
	rm -f $(EXAMPLES) *.c

# Show generated C code
show-%: %.joy
	$(PYTHON) $(JOY2C) $< --inline --fold --print
