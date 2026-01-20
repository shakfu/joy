#!/bin/bash
# Test runner script for Joy tests
# Usage: run_test.sh <joy_executable> [flags] <test_file>
# Exit code: 0 if all assertions pass (only 'true' lines), 1 otherwise

JOY_EXE="$1"
shift

# Last argument is test file, rest are flags
FLAGS=()
while [ $# -gt 1 ]; do
    FLAGS+=("$1")
    shift
done
TEST_FILE="$1"

if [ ! -x "$JOY_EXE" ]; then
    echo "Error: Joy executable not found or not executable: $JOY_EXE"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]; then
    echo "Error: Test file not found: $TEST_FILE"
    exit 1
fi

# Run the test and capture output
if [ ${#FLAGS[@]} -gt 0 ]; then
    OUTPUT=$("$JOY_EXE" "${FLAGS[@]}" "$TEST_FILE" 2>&1)
else
    OUTPUT=$("$JOY_EXE" "$TEST_FILE" 2>&1)
fi
EXIT_CODE=$?

# Check if joy crashed
if [ $EXIT_CODE -ne 0 ]; then
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

# Check for runtime errors
if echo "$OUTPUT" | grep -q "run time error"; then
    echo "FAIL: Runtime error"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

echo "PASS"
exit 0
