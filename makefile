#
#   module  : makefile
#   version : 2.0
#   date    : 02/18/25
#
.PHONY: all joy clean clang-format clang-format-check clang-tidy

BUILD_DIR ?= build
FORMAT_FILES := $(shell git ls-files '*.c' '*.h')
CLANG_TIDY ?= /opt/homebrew/opt/llvm/bin/clang-tidy

all: joy

joy:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) --target $@

clean:
	rm -rf $(BUILD_DIR)

clang-format:
	clang-format -i $(FORMAT_FILES)

clang-format-check:
	clang-format --dry-run --Werror $(FORMAT_FILES)

clang-tidy:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	python3 tools/run_clang_tidy.py $(BUILD_DIR) $(CLANG_TIDY)
