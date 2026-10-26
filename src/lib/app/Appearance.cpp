//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Appearance.hpp"
using namespace fbide;

auto fbide::parseAppearance(const wxString& value) -> std::optional<wxApp::Appearance> {
    const auto lower = value.Lower();
    if (lower == "system") {
        return wxApp::Appearance::System;
    }
    if (lower == "dark") {
        return wxApp::Appearance::Dark;
    }
    if (lower == "light") {
        return wxApp::Appearance::Light;
    }
    return std::nullopt;
}

auto fbide::appearanceConfigKey(const wxApp::Appearance appearance) -> wxString {
    switch (appearance) {
    case wxApp::Appearance::System:
        return "system";
    case wxApp::Appearance::Dark:
        return "dark";
    case wxApp::Appearance::Light:
        return "light";
    }
    std::unreachable();
}

auto fbide::isAppearanceSupported() -> bool {
#if defined(__WXMSW__)
    // wxMSWImpl::InitDarkMode: undocumented uxtheme APIs, 1903 and later.
    #if wxUSE_DARK_MODE
    return wxCheckOsVersion(10, 0, 18362);
    #else
    return false;
    #endif
#elif defined(__WXOSX__)
    // NSAppearance switching needs macOS 10.14 (Mojave).
    return wxCheckOsVersion(10, 14);
#elif defined(__WXGTK3__)
    // Switches through the desktop settings portal; when the portal is
    // missing, SetAppearance fails at startup and logs why.
    return true;
#else
    return false;
#endif
}
