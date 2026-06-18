//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FbcDefines.hpp"
using namespace fbide;

namespace {

auto toLower(std::string str) -> std::string {
    std::ranges::transform(str, str.begin(), [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });
    return str;
}

} // namespace

auto fbide::parseFbcDefines(const wxArrayString& lines) -> std::unordered_set<std::string> {
    constexpr std::string_view marker = "FBDEF ";
    constexpr std::string_view layout = " \t\r\n";

    std::unordered_set<std::string> defines;
    for (const auto& wline : lines) {
        const std::string line = wline.utf8_string();
        const auto pos = line.find(marker);
        if (pos == std::string::npos) {
            continue;
        }
        std::string_view rest { line };
        rest.remove_prefix(pos + marker.size());
        const auto start = rest.find_first_not_of(layout);
        if (start == std::string_view::npos) {
            continue;
        }
        rest.remove_prefix(start);
        rest = rest.substr(0, rest.find_first_of(layout));
        if (!rest.empty()) {
            defines.insert(toLower(std::string { rest }));
        }
    }
    return defines;
}
