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

/// Convert a `wxString` to a `std::filesystem::path`, picking the right
/// platform encoding (wide on Windows, UTF-8 elsewhere). Use at the
/// boundary when handing a path off to std::filesystem APIs.
FBIDE_INLINE static auto toPath(const wxString& str) -> std::filesystem::path {
#ifdef __WXMSW__
    return std::filesystem::path { str.ToStdWstring() };
#else
    return std::filesystem::path { str.ToStdString(wxConvUTF8) };
#endif
}

/// Convert a `std::filesystem::path` to a `wxString`, picking the right
/// platform encoding. Use at the boundary when handing a path to wx
/// APIs (wxFile streams, wxLog formatting, anything wxString-typed).
FBIDE_INLINE static auto toWx(const std::filesystem::path& path) -> wxString {
#ifdef __WXMSW__
    return wxString { path.wstring() };
#else
    return wxString::FromUTF8(path.string());
#endif
}

template<typename... Base>
struct Visitor : Base... {
    using Base::operator()...;
};

} // namespace fbide
