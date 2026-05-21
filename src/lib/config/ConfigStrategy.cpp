//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigStrategy.hpp"
using namespace fbide;

auto ConfigStrategy::deriveOverlayPath(
    const wxString& basePath,
    const wxString& userDataDir,
    const bool readOnly
) -> wxString {
    wxFileName fn(basePath);
    // `config_macos` → `config_macos.local`, extension stays put. Works
    // for any extension (`.ini`, legacy `.fbt`, ...) — overlay file ends
    // up `<stem>.local.<ext>`.
    fn.SetName(fn.GetName() + ".local");
    if (readOnly) {
        fn.SetPath(userDataDir);
    }
    return fn.GetFullPath();
}

auto ConfigStrategy::select(
    const wxString& basePath,
    const wxString& userDataDir,
    const bool readOnly,
    const bool overlayCapable,
    const bool explicitMode
) -> ConfigStrategy {
    if (!overlayCapable || explicitMode) {
        return direct(basePath);
    }
    return overlay(basePath, deriveOverlayPath(basePath, userDataDir, readOnly));
}
