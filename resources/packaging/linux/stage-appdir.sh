#!/usr/bin/env bash
#
# Stage the AppDir payload for the Linux AppImage: app + document icons, the
# .desktop entry, the shared-mime-info definitions and the AppStream metainfo.
# Invoked from .github/workflows/ci.yml after `cmake --install` has populated
# <appdir>/usr; runnable locally to reproduce the AppImage payload.
#
# Usage: stage-appdir.sh <appdir> <repo-root>

set -euo pipefail

APPDIR="$1"
REPO="$2"
IMAGES="$REPO/resources/images"
PKG="$REPO/resources/packaging/linux"

# hicolor icon sizes we ship (square). The source PNG sets carry 16..1024;
# hicolor has no standard 1024 apps dir, so cap at 512.
SIZES=(16 24 32 48 64 128 256 512)

# Copy <src> to <dst>, creating parent dirs and applying <mode>. Portable
# across GNU and BSD coreutils (`install -D` is a GNU-only extension).
install_file() {
    local src="$1" dst="$2" mode="$3"
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst"
    chmod "$mode" "$dst"
}

# Document-type icons as "<png-folder>:<icon-name>". The icon name is the MIME
# type with '/' replaced by '-' (freedesktop convention). Keep this mapping in
# sync with fbide.xml and fbide.desktop.
DOC_ICONS=(
    "file-bas:text-x-freebasic"
    "file-bi:text-x-freebasic-header"
    "file-fbs:application-x-fbide-session"
)

# App icon, every size.
for size in "${SIZES[@]}"; do
    install_file "$IMAGES/fbide/${size}.png" \
        "$APPDIR/usr/share/icons/hicolor/${size}x${size}/apps/fbide.png" 644
done

# Per-document-type icons, every size, under the mimetypes theme directory.
for entry in "${DOC_ICONS[@]}"; do
    folder="${entry%%:*}"
    icon="${entry#*:}"
    for size in "${SIZES[@]}"; do
        install_file "$IMAGES/${folder}/${size}.png" \
            "$APPDIR/usr/share/icons/hicolor/${size}x${size}/mimetypes/${icon}.png" 644
    done
done

# Top-level icon — what AppRun greps against and AppImage tooling expects at
# the AppDir root.
install_file "$IMAGES/fbide/256.png" "$APPDIR/fbide.png" 644

# MIME-type definitions so the desktop's MimeType= entries resolve to types
# (and thus to the mimetypes icons installed above).
install_file "$PKG/fbide.xml" "$APPDIR/usr/share/mime/packages/fbide.xml" 644

# Desktop entry, both at the FHS location and the AppDir root.
install_file "$PKG/fbide.desktop" "$APPDIR/usr/share/applications/fbide.desktop" 644
cp "$PKG/fbide.desktop" "$APPDIR/fbide.desktop"

# AppStream MetaInfo. The component id (reverse-DNS) doubles as the file's
# basename — software centres key off the path, so the leaf must match the
# <id> inside the XML.
install_file "$PKG/fbide.metainfo.xml" \
    "$APPDIR/usr/share/metainfo/io.github.albeva.fbide.metainfo.xml" 644

install_file "$PKG/AppRun" "$APPDIR/AppRun" 755
