#!/usr/bin/env bash
#
# Per-user desktop integration for a locally-built fbide (NOT an AppImage):
# installs the app icon, the per-document-type icons, the .desktop launcher and
# the shared-mime-info definitions into $XDG_DATA_HOME (~/.local/share by
# default), then refreshes the desktop / mime / icon caches.
#
# Unlike the AppImage path (resources/packaging/linux/stage-appdir.sh +
# FileAssociationsLinux::ensureRegistered, which only fire when $APPIMAGE is
# set), a plain `cmake --build` binary is not on PATH and never self-registers.
# This script wires it into the desktop for development use, pointing Exec= at
# the absolute path of the built binary.
#
# Usage:
#   install-local.sh <repo-root> <fbide-binary> [--prefix <share-dir>]
#   install-local.sh --uninstall [--prefix <share-dir>]
#
# <share-dir> defaults to $XDG_DATA_HOME, or ~/.local/share when unset.

set -euo pipefail

# hicolor icon sizes we ship (square); source PNG sets carry 16..1024, hicolor
# has no standard 1024 apps dir so cap at 512. Kept in sync with stage-appdir.sh.
SIZES=(16 24 32 48 64 128 256 512)

# Document-type icons as "<png-folder>:<icon-name>"; the icon name is the MIME
# type with '/' replaced by '-'. Kept in sync with fbide.xml / fbide.desktop /
# stage-appdir.sh.
DOC_ICONS=(
    "file-bas:text-x-freebasic"
    "file-bi:text-x-freebasic-header"
    "file-fbs:application-x-fbide-session"
)

DESKTOP_NAME="fbide.desktop"
MIME_NAME="fbide.xml"
APP_ICON="fbide"   # icon basename under apps/, matches Icon= in fbide.desktop

# --- argument parsing -------------------------------------------------------

UNINSTALL=0
PREFIX="${XDG_DATA_HOME:-$HOME/.local/share}"
POSITIONAL=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --uninstall) UNINSTALL=1; shift ;;
        --prefix)    PREFIX="${2:?--prefix needs a directory}"; shift 2 ;;
        --prefix=*)  PREFIX="${1#*=}"; shift ;;
        -h|--help)
            sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*) echo "unknown option: $1" >&2; exit 2 ;;
        *)  POSITIONAL+=("$1"); shift ;;
    esac
done

# Refresh the desktop / mime / icon caches under $PREFIX. Each tool is optional
# and its absence is not fatal — the assets still resolve, just less eagerly.
refresh_caches() {
    command -v update-desktop-database >/dev/null 2>&1 \
        && update-desktop-database "$PREFIX/applications" >/dev/null 2>&1 || true
    command -v update-mime-database >/dev/null 2>&1 \
        && update-mime-database "$PREFIX/mime" >/dev/null 2>&1 || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 \
        && gtk-update-icon-cache -f -t "$PREFIX/icons/hicolor" >/dev/null 2>&1 || true
}

# --- uninstall --------------------------------------------------------------

if [[ "$UNINSTALL" -eq 1 ]]; then
    rm -f "$PREFIX/applications/$DESKTOP_NAME"
    rm -f "$PREFIX/mime/packages/$MIME_NAME"
    for size in "${SIZES[@]}"; do
        rm -f "$PREFIX/icons/hicolor/${size}x${size}/apps/${APP_ICON}.png"
        for entry in "${DOC_ICONS[@]}"; do
            rm -f "$PREFIX/icons/hicolor/${size}x${size}/mimetypes/${entry#*:}.png"
        done
    done
    refresh_caches
    echo "Removed fbide desktop integration from $PREFIX"
    exit 0
fi

# --- install ----------------------------------------------------------------

if [[ "${#POSITIONAL[@]}" -ne 2 ]]; then
    echo "usage: install-local.sh <repo-root> <fbide-binary> [--prefix <share-dir>]" >&2
    exit 2
fi

REPO="${POSITIONAL[0]}"
BINARY="${POSITIONAL[1]}"
IMAGES="$REPO/resources/images"
PKG="$REPO/resources/packaging/linux"

[[ -d "$IMAGES" ]]   || { echo "no images dir: $IMAGES" >&2; exit 1; }
[[ -x "$BINARY" ]]   || { echo "binary not found/executable: $BINARY" >&2; exit 1; }

# Absolute Exec target — a local build is not on PATH, so the launcher must name
# the binary by full path (the AppImage's `Exec=fbide` would not resolve).
BINARY_ABS="$(cd "$(dirname "$BINARY")" && pwd)/$(basename "$BINARY")"

# Copy <src> to <dst>, creating parent dirs and applying <mode>.
install_file() {
    local src="$1" dst="$2" mode="$3"
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst"
    chmod "$mode" "$dst"
}

# App icon, every size.
for size in "${SIZES[@]}"; do
    install_file "$IMAGES/$APP_ICON/${size}.png" \
        "$PREFIX/icons/hicolor/${size}x${size}/apps/${APP_ICON}.png" 644
done

# Per-document-type icons, every size, under the mimetypes theme directory.
for entry in "${DOC_ICONS[@]}"; do
    folder="${entry%%:*}"
    icon="${entry#*:}"
    for size in "${SIZES[@]}"; do
        install_file "$IMAGES/${folder}/${size}.png" \
            "$PREFIX/icons/hicolor/${size}x${size}/mimetypes/${icon}.png" 644
    done
done

# MIME-type definitions so the launcher's MimeType= entries resolve to types
# (and thus to the mimetypes icons above).
install_file "$PKG/$MIME_NAME" "$PREFIX/mime/packages/$MIME_NAME" 644

# Desktop entry, with Exec= rewritten to the absolute built binary.
mkdir -p "$PREFIX/applications"
sed "s|^Exec=.*|Exec=$BINARY_ABS %F|" "$PKG/$DESKTOP_NAME" \
    > "$PREFIX/applications/$DESKTOP_NAME"
chmod 644 "$PREFIX/applications/$DESKTOP_NAME"

refresh_caches

echo "Installed fbide desktop integration into $PREFIX"
echo "  launcher: $PREFIX/applications/$DESKTOP_NAME (Exec=$BINARY_ABS)"
