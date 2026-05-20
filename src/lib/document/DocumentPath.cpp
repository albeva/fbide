//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentPath.hpp"

namespace fbide {

auto toFsPath(const wxString& s) -> std::filesystem::path {
#ifdef _WIN32
    return std::filesystem::path { s.wc_str() };
#else
    return std::filesystem::path { s.utf8_string() };
#endif
}

auto toWxString(const std::filesystem::path& p) -> wxString {
#ifdef _WIN32
    return wxString { p.wstring() };
#else
    return wxString::FromUTF8(p.string());
#endif
}

auto canonicalizePath(const std::filesystem::path& path) -> std::filesystem::path {
    if (path.empty()) {
        return path;
    }

    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }

    canonical.make_preferred();
    return canonical;
}

} // namespace fbide
