#!/usr/bin/env bash
#
# Generate the macOS icon artifacts committed under src/lib/rc/. Run by hand
# (like make-doc-icons.py) whenever the source art changes; the normal CMake
# build just copies the committed output and never needs Xcode/actool.
#
# App icon:  resources/images/fbide.icon (Icon Composer) -> actool ->
#            Assets.car (liquid-glass icon, referenced by CFBundleIconName) +
#            fbide.icns (classic fallback, CFBundleIconFile).
# Doc icons: resources/images/file-<type>/<size>.png -> iconutil ->
#            file-<type>.icns (CFBundleTypeIconFile per document type).
#
# Requires: Xcode (actool) and iconutil. macOS only.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGES_DIR="$REPO_ROOT/resources/images"
OUT_DIR="$REPO_ROOT/src/lib/rc"

# Document types that get their own icon. Keep in sync with the
# CFBundleDocumentTypes entries in configured_files/Info.plist.in and the
# Windows doc_* icons in src/lib/rc/app.rc.
DOC_TYPES=(file-bas file-bi file-fbs)

# Min deployment target — matches LSMinimumSystemVersion in Info.plist.in.
DEPLOYMENT_TARGET=11.0

# macOS .iconset slots: "<srcsize>:<iconset-name>". A .icns set is built from
# the 16/32/128/256/512 base sizes plus their @2x doubles, so several source
# PNGs feed two slots.
ICONSET_SLOTS=(
    "16:icon_16x16.png"
    "32:icon_16x16@2x.png"
    "32:icon_32x32.png"
    "64:icon_32x32@2x.png"
    "128:icon_128x128.png"
    "256:icon_128x128@2x.png"
    "256:icon_256x256.png"
    "512:icon_256x256@2x.png"
    "512:icon_512x512.png"
    "1024:icon_512x512@2x.png"
)

make_doc_icon() {
    local type="$1"
    local src_dir="$IMAGES_DIR/$type"
    local iconset; iconset="$(mktemp -d)/${type}.iconset"
    mkdir -p "$iconset"

    local slot src name
    for slot in "${ICONSET_SLOTS[@]}"; do
        src="${slot%%:*}"
        name="${slot#*:}"
        cp "$src_dir/${src}.png" "$iconset/$name"
    done

    iconutil --convert icns --output "$OUT_DIR/${type}.icns" "$iconset"
    rm -rf "$(dirname "$iconset")"
    echo "  $OUT_DIR/${type}.icns"
}

make_app_icon() {
    local tmp; tmp="$(mktemp -d)"
    # actool emits Assets.car + fbide.icns named after --app-icon. The partial
    # plist it writes is discarded: the icon keys are hardcoded in Info.plist.in.
    xcrun actool \
        --compile "$tmp" \
        --app-icon fbide \
        --output-partial-info-plist "$tmp/partial.plist" \
        --platform macosx \
        --minimum-deployment-target "$DEPLOYMENT_TARGET" \
        "$IMAGES_DIR/fbide.icon" >/dev/null
    cp "$tmp/Assets.car" "$OUT_DIR/Assets.car"
    cp "$tmp/fbide.icns" "$OUT_DIR/fbide.icns"
    rm -rf "$tmp"
    echo "  $OUT_DIR/Assets.car"
    echo "  $OUT_DIR/fbide.icns"
}

echo "App icon:"
make_app_icon

echo "Document icons:"
for type in "${DOC_TYPES[@]}"; do
    make_doc_icon "$type"
done
