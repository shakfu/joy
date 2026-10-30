#!/bin/bash
# Test runner script for Joy tests
# Usage: run_test.sh <joy_executable> <source_dir> [flags] <test_file>
# Exit code: 0 if all assertions pass (only 'true' lines), 1 otherwise
# ALLOW_ERRORS=1: runtime errors are expected; only assertions are checked

JOY_EXE="$1"
shift
SOURCE_DIR="$1"
shift

# Last argument is test file, rest are flags
FLAGS=()
while [ $# -gt 1 ]; do
    FLAGS+=("$1")
    shift
done
TEST_FILE="$1"

# usrlib.joy is the default startup library: it loads inilib.joy and agglib.joy
STARTLIB="$SOURCE_DIR/lib/usrlib.joy"

if [ ! -x "$JOY_EXE" ]; then
    echo "Error: Joy executable not found or not executable: $JOY_EXE"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]; then
    echo "Error: Test file not found: $TEST_FILE"
    exit 1
fi

# Run the test and capture output.
# Only the first filename argument is executed (later ones become argv), so
# inilib.joy and the test are included from stdin. The cwd is the test's
# directory so relative paths in tests resolve.
TEST_DIR=$(cd "$(dirname "$TEST_FILE")" && pwd)
TEST_PATH="$TEST_DIR/$(basename "$TEST_FILE")"
if [ -f "$STARTLIB" ]; then
    STARTLIB=$(cd "$(dirname "$STARTLIB")" && pwd)/$(basename "$STARTLIB")
    SCRIPT=$(printf '"%s" include.\n"%s" include.\n' "$STARTLIB" "$TEST_PATH")
else
    SCRIPT=$(printf '"%s" include.\n' "$TEST_PATH")
fi
OUTPUT=$(cd "$TEST_DIR" && printf '%s\n' "$SCRIPT" | "$JOY_EXE" "${FLAGS[@]}" 2>&1)
EXIT_CODE=$?

# Check if joy crashed or failed
if [ $EXIT_CODE -gt 128 ] || { [ $EXIT_CODE -ne 0 ] && [ -z "$ALLOW_ERRORS" ]; }; then
    echo "FAIL: Joy exited with code $EXIT_CODE"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

# Check for 'false' in output (indicates assertion failure)
if echo "$OUTPUT" | grep -q "^false$"; then
    echo "FAIL: Test assertions failed"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

# Executing a bad node means memory corruption, even when errors are expected
if echo "$OUTPUT" | grep -q "attempting to execute bad node"; then
    echo "FAIL: Bad node executed (stale index after GC)"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

# Check for runtime errors
if [ -z "$ALLOW_ERRORS" ] && echo "$OUTPUT" | grep -q "run time error"; then
    echo "FAIL: Runtime error"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

# With expected errors, require the last assertion to have run
if [ -n "$ALLOW_ERRORS" ] && [ "$(echo "$OUTPUT" | tail -n 1)" != "true" ]; then
    echo "FAIL: Test did not reach its final assertion"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

echo "PASS"
exit 0
