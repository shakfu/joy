#!/bin/bash
# session-diff under GC pressure: lists built first must survive later allocation

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
JOY="${JOY:-$PROJECT_DIR/build/joy}"
TESTDIR="$(mktemp -d)"
trap 'rm -rf "$TESTDIR"' EXIT
cd "$TESTDIR"

N=3000
{
    echo '"current" session.'
    for i in $(seq $N); do echo "DEFINE mod$i == 1. DEFINE gone$i == 1."; done
    echo 'session-close.'
    echo '"other" session.'
    for i in $(seq $N); do echo "DEFINE mod$i == 2. DEFINE new$i == 1."; done
    echo 'session-close.'
    echo '"current" session.'
    echo '"other" session-diff'
    echo 'dup "added" dget size put "\n" putchars'
    echo 'dup "modified" dget size put "\n" putchars'
    echo 'dup "removed" dget size put "\n" putchars'
    echo '"added" dget first put "\n" putchars.'
} > input.joy

OUTPUT=$("$JOY" < input.joy 2>&1)
STATUS=$?
COUNTS=$(echo "$OUTPUT" | grep -E '^[0-9]+$' | tr '\n' ' ')
if ! echo "$OUTPUT" | grep -qE '^"new[0-9]+"$'; then
    echo "=== TEST FAILED: added list does not hold symbol names ==="
    echo "$OUTPUT" | tail -5 | cut -c1-200
    exit 1
fi
EXPECTED="$N $N $N "
if [ "$COUNTS" != "$EXPECTED" ]; then
    echo "=== TEST FAILED: counts '$COUNTS', expected '$EXPECTED' (exit $STATUS) ==="
    echo "$OUTPUT" | tail -5 | cut -c1-200
    exit 1
fi
echo "=== session-diff OK ==="

# A dictionary in a definition reloads from the database
OUTPUT=$("$JOY" 2>&1 <<'JOY'
"dicts" session.
DEFINE d == {"k": [1 "s"] "n": {:}}.
session-close.
"dicts" session.
d "k" dget.
JOY
)
if ! echo "$OUTPUT" | grep -q '^\[1 "s"\]$'; then
    echo "=== TEST FAILED: dictionary did not reload ==="
    echo "$OUTPUT" | tail -5
    exit 1
fi
echo "=== dictionary reload OK ==="
