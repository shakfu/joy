#!/bin/bash
# Test session persistence with full deserialization

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
JOY="$PROJECT_DIR/build/joy"
TESTDIR="/tmp/joy_session_test_$$"
DBFILE="$TESTDIR/test_persist.joy.db"

# Create test directory
mkdir -p "$TESTDIR"
cd "$TESTDIR"

echo "=== Testing Session Persistence ==="

# Remove any existing session database
rm -f test_persist.joy.db

# Step 1: Create session and define values
echo "Step 1: Creating session and defining values..."
$JOY <<'EOF'
"test_persist" session.

(* Define various types *)
DEFINE my_int == 42.
DEFINE my_float == 3.14159.
DEFINE my_list == [1 2 3 4 5].
DEFINE my_nested == [[1 2] [3 4] [5 6]].
DEFINE my_quot == [dup *].
DEFINE my_string == "hello world".
DEFINE my_mixed == [1 "hi" 2.5 true].
DEFINE my_set == {1 2 3}.
DEFINE my_char == 'A.
DEFINE my_dict == [["name" "Joy"] ["version" 1]] >dict.

"Defined values in session.\n" putchars.
session-close.
EOF

echo "Step 1 complete."

# Step 2: Reopen session and verify values
echo ""
echo "Step 2: Reopening session and verifying values..."
RESULT=$($JOY <<'EOF'
"test_persist" session.

(* Verify integer *)
my_int 42 = "int_ok" "int_FAIL" choice putchars "\n" putchars.

(* Verify float *)
my_float 3.14159 = "float_ok" "float_FAIL" choice putchars "\n" putchars.

(* Verify list *)
my_list size 5 = "list_size_ok" "list_size_FAIL" choice putchars "\n" putchars.
my_list first 1 = "list_first_ok" "list_first_FAIL" choice putchars "\n" putchars.
my_list 0 [+] fold 15 = "list_sum_ok" "list_sum_FAIL" choice putchars "\n" putchars.

(* Verify nested list *)
my_nested size 3 = "nested_size_ok" "nested_size_FAIL" choice putchars "\n" putchars.
my_nested first size 2 = "nested_inner_ok" "nested_inner_FAIL" choice putchars "\n" putchars.

(* Verify quotation works *)
5 my_quot i 25 = "quot_exec_ok" "quot_exec_FAIL" choice putchars "\n" putchars.

(* Verify string *)
my_string "hello world" = "string_ok" "string_FAIL" choice putchars "\n" putchars.

(* Verify mixed list *)
my_mixed size 4 = "mixed_size_ok" "mixed_size_FAIL" choice putchars "\n" putchars.
my_mixed first 1 = "mixed_first_ok" "mixed_first_FAIL" choice putchars "\n" putchars.

(* Verify set *)
1 my_set in "set_1_ok" "set_1_FAIL" choice putchars "\n" putchars.
4 my_set in not "set_4_ok" "set_4_FAIL" choice putchars "\n" putchars.

(* Verify char *)
my_char 65 = "char_ok" "char_FAIL" choice putchars "\n" putchars.

(* Verify dict - note: dicts serialize as {...} but may need special handling *)
my_dict "name" dget "Joy" = "dict_name_ok" "dict_name_FAIL" choice putchars "\n" putchars.
my_dict "version" dget 1 = "dict_version_ok" "dict_version_FAIL" choice putchars "\n" putchars.

session-close.
EOF
)

echo "$RESULT"

# Check for failures
if echo "$RESULT" | grep -q "FAIL"; then
    echo ""
    echo "=== TEST FAILED ==="
    rm -rf "$TESTDIR"
    exit 1
fi

echo ""
echo "=== ALL TESTS PASSED ==="

# Cleanup
rm -rf "$TESTDIR"
exit 0
