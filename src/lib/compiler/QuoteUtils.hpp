//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Escape unescaped double quotes in a string.
inline auto escapeQuotes(const wxString& str) -> wxString {
    wxString escaped;
    escaped.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '"' && (i == 0 || str[i - 1] != '\\')) {
            escaped += '\\';
        }
        escaped += str[i];
    }
    return escaped;
}

/// Quote a value for command line use.
/// Trims whitespace, handles already-quoted values,
/// and escapes unescaped inner double quotes.
inline auto quoteArg(const wxString& val) -> wxString {
    const auto trimmed = val.Strip(wxString::both);
    if (trimmed.StartsWith('"') && trimmed.EndsWith('"')) {
        const auto inner = trimmed.Mid(1, trimmed.length() - 2);
        return "\"" + escapeQuotes(inner) + "\"";
    }
    return "\"" + escapeQuotes(trimmed) + "\"";
}

} // namespace fbide
