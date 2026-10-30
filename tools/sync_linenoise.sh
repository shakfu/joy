#!/bin/bash
# Update ext/linenoise from a linenoise checkout (default: ../linenoise).
# Usage: tools/sync_linenoise.sh [path/to/linenoise]
#
# Only files already vendored are copied; syntax/joy.{c,h} are Joy's own.
# Upstream keeps sources in src/ and headers in include/; ext/linenoise
# flattens that layout. The upstream commit is recorded in ext/linenoise/VERSION.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM="$(cd "${1:-$ROOT/../linenoise}" && pwd)"
DEST="$ROOT/ext/linenoise"

if ! git -C "$UPSTREAM" diff --quiet HEAD -- src include; then
    echo "error: $UPSTREAM has uncommitted changes in src/ or include/" >&2
    exit 1
fi

cd "$DEST"
find . -type f \( -name '*.c' -o -name '*.h' \) | sed 's|^\./||' | sort |
while read -r f; do
    case "$f" in
    syntax/joy.c | syntax/joy.h) continue ;;
    *.c) src="$UPSTREAM/src/$f" ;;
    *.h) src="$UPSTREAM/include/$f" ;;
    esac
    if [ ! -f "$src" ]; then
        echo "error: $src not found (layout changed upstream?)" >&2
        exit 1
    fi
    cp "$src" "$f"
done

git -C "$UPSTREAM" log -1 --format='%H %cs %s' > VERSION
echo "ext/linenoise now at: $(cat VERSION)"
echo "Rebuild, run make test, and check callback signatures in src/cli/main.c and syntax/joy.c."
