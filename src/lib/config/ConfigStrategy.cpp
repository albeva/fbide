//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigStrategy.hpp"
using namespace fbide;

auto ConfigStrategy::deriveOverlayPath(
    const std::filesystem::path& basePath,
    const std::filesystem::path& userDataDir,
    const bool readOnly
) -> std::filesystem::path {
    // `config_macos.ini` → `config_macos.local.ini`; extension stays
    // put. Works for any extension (`.ini`, legacy `.fbt`, ...) — the
    // overlay file ends up `<stem>.local.<ext>`.
    const auto parent = readOnly ? userDataDir : basePath.parent_path();
    auto filename = basePath.stem();
    filename += ".local";
    filename += basePath.extension();
    return parent / filename;
}

auto ConfigStrategy::select(
    const std::filesystem::path& basePath,
    const std::filesystem::path& userDataDir,
    const bool readOnly,
    const bool overlayCapable,
    const bool explicitMode
) -> ConfigStrategy {
    if (!overlayCapable || explicitMode) {
        return direct(basePath);
    }
    return overlay(basePath, deriveOverlayPath(basePath, userDataDir, readOnly));
}
