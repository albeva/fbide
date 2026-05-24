#!/bin/bash
# Bundle non-system dynamic libraries into a macOS .app so the bundle
# runs on machines without Homebrew installed.
#
# Built FBIde links against Homebrew LLVM's libc++ / libc++abi /
# libunwind (we use Homebrew clang for C++23 stdlib coverage that
# Apple's libc++ doesn't yet ship). The resulting binary's load
# commands point at Homebrew's absolute install paths — which only
# exist on the build machine. This script copies every non-system
# dylib into Contents/Frameworks/ and rewrites every install_name
# inside the bundle to use @executable_path/../Frameworks/ so the
# .app is fully self-contained.
#
# Implementation: dylibbundler does the discovery + copy + rewrite
# transitively. We then verify with otool -L that nothing outside
# the system paths or our Frameworks/ dir remains.
#
# Usage:
#   bundle-dylibs.sh <app-path>
#
# Requires:
#   dylibbundler (brew install dylibbundler)

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $0 <app-path>" >&2
    exit 2
fi

APP="$1"
if [ ! -d "$APP" ]; then
    echo "error: '$APP' is not a directory" >&2
    exit 1
fi

NAME="$(basename "$APP" .app)"
BIN="$APP/Contents/MacOS/$NAME"
if [ ! -f "$BIN" ]; then
    echo "error: missing executable at $BIN" >&2
    exit 1
fi

if ! command -v dylibbundler >/dev/null 2>&1; then
    echo "error: dylibbundler not on PATH (brew install dylibbundler)" >&2
    exit 1
fi

FRAMEWORKS="$APP/Contents/Frameworks"

# -cd: create the destination directory if missing.
# -of: overwrite files if rerun (idempotent local testing).
# -b:  bundle (copy + fix install names).
# -x:  binary to inspect for dependencies.
# -d:  destination Frameworks dir.
# -p:  install name prefix for the rewritten references.
dylibbundler \
    -cd -of -b \
    -x "$BIN" \
    -d "$FRAMEWORKS/" \
    -p '@executable_path/../Frameworks/'

# Verify: every load command must either be a system path
# (/usr/lib/*, /System/*) or already rewritten to @executable_path.
# Anything else means a dylib slipped through — fail loudly so CI
# catches it before the DMG ships.
deps=$(otool -L "$BIN" | tail -n +2 | awk '{print $1}')
bad=$(echo "$deps" | grep -vE '^(/usr/lib/|/System/|@executable_path/|@rpath/)' || true)
if [ -n "$bad" ]; then
    echo "error: $BIN has non-bundled, non-system deps:" >&2
    echo "$bad" >&2
    exit 1
fi

# Same check for every bundled dylib — transitive deps must also
# point inside the bundle or at the system.
while IFS= read -r dylib; do
    bad=$(otool -L "$dylib" | tail -n +2 | awk '{print $1}' \
        | grep -vE '^(/usr/lib/|/System/|@executable_path/|@rpath/)' \
        | grep -v "^$(basename "$dylib")" || true)
    if [ -n "$bad" ]; then
        echo "error: $dylib has unresolved deps:" >&2
        echo "$bad" >&2
        exit 1
    fi
done < <(find "$FRAMEWORKS" -type f -name '*.dylib' 2>/dev/null)

echo "bundled dylibs verified clean"
