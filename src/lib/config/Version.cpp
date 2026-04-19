//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Version.hpp"
using namespace fbide;

Version::Version(const wxString& version) noexcept {
    wxArrayString parts = wxSplit(version, '.');
    if (parts.size() > 0) {
        parts[0].ToInt(&m_major);
    };
    if (parts.size() > 1) {
        parts[1].ToInt(&m_minor);
    };
    if (parts.size() > 2) {
        parts[2].ToInt(&m_patch);
    };
}

auto Version::asString() const -> wxString {
    return wxString::Format("%d.%d.%d", m_major, m_minor, m_patch);
}
