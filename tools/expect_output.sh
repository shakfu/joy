# Sourced by run_test.sh and run_crash_test.sh.
# expect_output <test.joy> <output>: compare output with <test>.out, if present.
# A test without a .out file is not compared; nondeterministic and
# build-dependent tests have none. JOY_UPDATE_EXPECTED=1 rewrites existing
# .out files instead of comparing; create an empty .out to add a test.
expect_output() {
    local expected="${1%.joy}.out"
    [ -f "$expected" ] || return 0
    if [ -n "$JOY_UPDATE_EXPECTED" ]; then
        printf '%s\n' "$2" > "$expected"
        return 0
    fi
    local delta
    if ! delta=$(diff -u <(tr -d '\r' < "$expected") <(printf '%s\n' "$2" | tr -d '\r')); then
        echo "FAIL: Output differs from $expected"
        echo "$delta"
        return 1
    fi
}
