#!/bin/bash
# Package a built fbide.app as a compressed, read-only DMG with the
# conventional drag-to-Applications layout.
#
# Usage:
#   build-dmg.sh <app-path> <version> <arch> [output-dir]
#
# Args:
#   app-path    path to the .app bundle (e.g. bin/fbide.app)
#   version     fbide version string for the volume / file name
#               (e.g. 0.5.0 or 0.5.0.rc-3)
#   arch        target architecture (arm64 | x86_64) — embedded in
#               the file name so per-arch DMGs don't collide
#   output-dir  optional, default `.` — where the .dmg is written
#
# Output:
#   <output-dir>/fbide-<version>-macos-<arch>.dmg
#
# Implementation notes:
# - hdiutil is the system tool; no homebrew dependency.
# - UDZO = compressed, read-only UDIF. Standard distribution format.
# - The volume mounts as "FBIde <version>". HFS+ inside (UDZO default),
#   reads on macOS 11+ without flags.
# - zlib-level=9 wins ~5-10% over the default at the cost of a few
#   seconds per build — fine on CI, the artefact is downloaded many
#   times more than it's built.
# - The Applications symlink + .app side-by-side in the volume gives
#   users the conventional "drag the icon onto the folder" install.
# - Window background image / icon positions are deliberately *not*
#   styled here. That would need AppleScript / `osascript` and adds
#   significant CI complexity for a placeholder release.

set -euo pipefail

if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    echo "usage: $0 <app-path> <version> <arch> [output-dir]" >&2
    exit 2
fi

APP_PATH="$1"
VERSION="$2"
ARCH="$3"
OUT_DIR="${4:-.}"

case "$ARCH" in
    arm64|x86_64) ;;
    *)
        echo "error: arch must be arm64 or x86_64, got '$ARCH'" >&2
        exit 1
        ;;
esac

if [ ! -d "$APP_PATH" ]; then
    echo "error: '$APP_PATH' is not a directory" >&2
    exit 1
fi
if [ ! -f "$APP_PATH/Contents/Info.plist" ]; then
    echo "error: '$APP_PATH' doesn't look like a .app bundle (missing Contents/Info.plist)" >&2
    exit 1
fi

APP_NAME="$(basename "$APP_PATH")"
DMG_NAME="fbide-${VERSION}-macos-${ARCH}.dmg"
VOL_NAME="FBIde ${VERSION} ${ARCH}"

mkdir -p "$OUT_DIR"
DMG_OUT="$(cd "$OUT_DIR" && pwd)/$DMG_NAME"

# Stage in a private temp dir so a failed run can't leave a half-built
# dmg-staging next to the workspace.
STAGING="$(mktemp -d -t fbide-dmg-XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT

# Copy the .app (preserving symlinks/permissions) and drop a symlink to
# /Applications so the mounted volume offers the drag-to-install layout.
cp -R "$APP_PATH" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

# Build the DMG. -ov overwrites a stale output; -fs is implicit HFS+ for
# UDZO.  -imagekey zlib-level=9 squeezes the most out of the compressor.
rm -f "$DMG_OUT"
hdiutil create \
    -volname "$VOL_NAME" \
    -srcfolder "$STAGING" \
    -ov \
    -format UDZO \
    -imagekey zlib-level=9 \
    "$DMG_OUT" >/dev/null

# Smoke-check the result before declaring success.
hdiutil verify "$DMG_OUT" >/dev/null

echo "$DMG_OUT"
