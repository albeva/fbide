//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Parse the `appearance=` config value (`system` / `dark` / `light`, any
/// case). Empty (leave the platform default alone) and unknown values give
/// nullopt.
[[nodiscard]] auto parseAppearance(const wxString& value) -> std::optional<wxApp::Appearance>;

/// The `appearance=` config value for `appearance`.
[[nodiscard]] auto appearanceConfigKey(wxApp::Appearance appearance) -> wxString;

/// True when `wxApp::SetAppearance` can switch light/dark on this system.
/// wxWidgets has no public query for this, so it mirrors wx's own conditions:
/// Windows 10 1903 (build 18362) or later, macOS 10.14 or later, GTK3. Older
/// systems (e.g. Windows 7) can't switch, so the setting is hidden there.
[[nodiscard]] auto isAppearanceSupported() -> bool;

} // namespace fbide
