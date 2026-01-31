#!/bin/bash
# Test session snapshots with complex values

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
JOY="$PROJECT_DIR/build/joy"
TESTDIR="/tmp/joy_session_snapshot_test_$$"

# Create test directory
mkdir -p "$TESTDIR"
cd "$TESTDIR"

echo "=== Testing Session Snapshots ==="

# Remove any existing session database
rm -f test_snap.joy.db

# Step 1: Create session, define v1 values, take snapshot
echo "Step 1: Creating session and v1 snapshot..."
$JOY <<'EOF'
"test_snap" session.

DEFINE my_list == [1 2 3].
DEFINE my_quot == [dup +].

"v1" snapshot.
"Created v1 snapshot.\n" putchars.
session-close.
EOF

# Step 2: Modify values, take v2 snapshot
echo "Step 2: Modifying values and creating v2 snapshot..."
$JOY <<'EOF'
"test_snap" session.

DEFINE my_list == [10 20 30 40 50].
DEFINE my_quot == [dup *].

"v2" snapshot.
"Created v2 snapshot.\n" putchars.
session-close.
EOF

# Step 3: Restore v1 and verify
echo "Step 3: Restoring v1 and verifying..."
RESULT=$($JOY <<'EOF'
"test_snap" session.

"v1" restore.

my_list size 3 = "v1_list_size_ok" "v1_list_size_FAIL" choice putchars "\n" putchars.
my_list first 1 = "v1_list_first_ok" "v1_list_first_FAIL" choice putchars "\n" putchars.
5 my_quot i 10 = "v1_quot_ok" "v1_quot_FAIL" choice putchars "\n" putchars.

session-close.
EOF
)
echo "$RESULT"

if echo "$RESULT" | grep -q "FAIL"; then
    echo "=== v1 RESTORE FAILED ==="
    rm -rf "$TESTDIR"
    exit 1
fi

# Step 4: Restore v2 and verify
echo "Step 4: Restoring v2 and verifying..."
RESULT=$($JOY <<'EOF'
"test_snap" session.

"v2" restore.

my_list size 5 = "v2_list_size_ok" "v2_list_size_FAIL" choice putchars "\n" putchars.
my_list first 10 = "v2_list_first_ok" "v2_list_first_FAIL" choice putchars "\n" putchars.
5 my_quot i 25 = "v2_quot_ok" "v2_quot_FAIL" choice putchars "\n" putchars.

session-close.
EOF
)
echo "$RESULT"

if echo "$RESULT" | grep -q "FAIL"; then
    echo "=== v2 RESTORE FAILED ==="
    rm -rf "$TESTDIR"
    exit 1
fi

echo ""
echo "=== ALL SNAPSHOT TESTS PASSED ==="

# Cleanup
rm -rf "$TESTDIR"
exit 0
