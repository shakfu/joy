#!/bin/bash
# Session failures raise errors and leave no half-open session or attached database

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
JOY="${JOY:-$PROJECT_DIR/build/joy}"
TESTDIR="$(mktemp -d)"
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

head -c 4096 /dev/urandom > bad.joy.db
OUTPUT=$("$JOY" 2>&1 <<'JOY'
"bad" session.
session-close.
"other" session.
DEFINE b == 2.
"good" session.
"bad" session-merge.
"other" session-merge.
JOY
)
echo "$OUTPUT"

expect() {
    if ! echo "$OUTPUT" | grep -q "$1"; then
        echo "=== TEST FAILED: missing '$1' ==="
        exit 1
    fi
}
expect "usable session database (file is not a database) needed for session"
expect "session: no active session"
expect "session-merge: merged 'other' successfully"
echo "=== TEST PASSED ==="
