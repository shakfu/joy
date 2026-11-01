#!/bin/bash
# ffi failures raise errors before any foreign call runs
JOY="${JOY:?JOY must name the interpreter}"

if [ -z "$JOY_FFI_TESTLIB" ]; then
    OUTPUT=$(echo '"" "abs" [int] [int] ffi.' | "$JOY" 2>&1)
    echo "$OUTPUT" | grep -q "a build with -DJOY_FFI=ON needed for ffi" || {
        echo "=== TEST FAILED: $OUTPUT ==="
        exit 1
    }
    echo "=== ffi disabled error OK ==="
    exit 0
fi

check() {
    OUTPUT=$(printf '%s\n' "$1" | "$JOY" 2>&1)
    if ! echo "$OUTPUT" | grep -q -- "$2"; then
        echo "=== TEST FAILED: '$1' ==="
        echo "expected: $2"
        echo "got: $OUTPUT"
        exit 1
    fi
}

L="\"$JOY_FFI_TESTLIB\""
check '"/no/such/lib" "f" [] [] ffi.' 'loadable library'
check "$L \"no_such_symbol\" [] [] ffi." 'symbol in library'
check "$L \"count\" [nosuchtype] [] ffi." 'known argument type'
check "$L \"count\" [] [nosuchtype] ffi." 'one known return type'
check "$L \"count\" [] [int int] ffi." 'one known return type'
check "$L \"count\" [int int int int int int int int int int int int int int int int int] [] ffi." 'at most 16'
check '1 2 3 ffi.' 'needed for ffi'
check '1 "s" [] [] ffi.' 'library and symbol strings'
check '"" "s" 1 [] ffi.' 'two type lists'
check "2 $L \"add_int\" [int int] [int] ffi-call." 'more arguments'
check "1 \"x\" $L \"add_int\" [int int] [int] ffi-call." 'integer argument'
check "1 2147483648 $L \"add_int\" [int int] [int] ffi-call." 'in range of its C type'
check "-1 $L \"same_pointer\" [uint] [int] ffi-call." 'in range of its C type'
check "1 $L \"size_of_string\" [string] [size_t] ffi-call." 'string argument'
check "'a $L \"half_double\" [double] [double] ffi-call." 'numeric argument'
check "1 $L \"negate\" [bool] [bool] ffi-call." 'logical argument'
check "$L \"null_string\" [] [string] ffi-call." 'non-NULL string result'
check "$L \"big_uint64\" [] [uint64] ffi-call." 'result in range of integer'
echo "=== ffi errors OK ==="
