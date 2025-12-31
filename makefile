#
#   module  : makefile
#   version : 2.0
#   date    : 02/18/25
#
.PHONY: all joy clean

BUILD_DIR ?= build

all: joy

joy:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target $@

clean:
	rm -rf $(BUILD_DIR)
