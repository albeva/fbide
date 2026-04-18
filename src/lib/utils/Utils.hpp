//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {
/**
 * Concatinate path component together separated by platform specific path
 * component separator
 */
FBIDE_INLINE static wxString operator/(const wxString& lhs, const wxString& rhs) {
    return wxString(lhs).append(wxFileName::GetPathSeparator()).append(rhs);
}

FBIDE_INLINE static wxString operator/=(const wxString& lhs, const wxString& rhs) {
    return wxString(lhs).append(wxFileName::GetPathSeparator()).append(rhs);
}

/**
 * wxString shorthand. "Hello"_wx
 */
FBIDE_INLINE static wxString operator""_wx(const char* s, std::size_t len) {
    return { s, len };
}

template<typename... Base>
struct Visitor : Base... {
    using Base::operator()...;
};

} // namespace fbide
