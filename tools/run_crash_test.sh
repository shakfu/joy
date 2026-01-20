#!/bin/bash
# Crash test runner for Joy tests (test4)
# Usage: run_crash_test.sh <joy_executable> [flags] <test_file>
# Exit code: 0 if interpreter doesn't crash (segfault), 1 otherwise
# This runner tolerates runtime errors but not crashes

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

# Check for crashes (signals like SIGSEGV, SIGBUS, SIGABRT)
# These typically result in exit codes > 128
if [ $EXIT_CODE -gt 128 ]; then
    SIGNAL=$((EXIT_CODE - 128))
    echo "FAIL: Joy crashed with signal $SIGNAL"
    echo "Output:"
    echo "$OUTPUT"
    exit 1
fi

echo "PASS"
exit 0
