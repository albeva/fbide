//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Convert a `wxString` to `std::filesystem::path`. Platform-correct:
/// uses wide chars on Windows and UTF-8 elsewhere — matches the
/// encoding `wxString` keeps internally on each platform. Use at any
/// boundary where a `wxString` carrying a file path meets a
/// `std::filesystem` API.
[[nodiscard]] inline auto toFsPath(const wxString& str) -> std::filesystem::path {
#ifdef __WXMSW__
    return std::filesystem::path { str.ToStdWstring() };
#else
    return std::filesystem::path { str.utf8_string() };
#endif
}

/// Inverse of `toFsPath`. Use at any boundary where an
/// `std::filesystem::path` meets a wx API (dialog title, log message,
/// tab title, `wxFile` constructor, ...).
[[nodiscard]] inline auto toWxString(const std::filesystem::path& path) -> wxString {
#ifdef __WXMSW__
    return wxString { path.wstring() };
#else
    return wxString::FromUTF8(path.string());
#endif
}

} // namespace fbide
