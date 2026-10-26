//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatIO.hpp"
using namespace fbide;

namespace {

auto equalsIgnoreAsciiCase(const std::string_view lhs, const std::string_view rhs) -> bool {
    const auto lower = [](const char ch) {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
    };
    return lhs.size() == rhs.size()
        && std::ranges::equal(lhs, rhs, [&](const char a, const char b) { return lower(a) == lower(b); });
}

} // namespace

auto fbide::parseEncodingName(const std::string_view name) -> std::optional<TextEncoding> {
    for (const auto value : TextEncoding::all) {
        const TextEncoding encoding { value };
        if (equalsIgnoreAsciiCase(encoding.toString(), name)) {
            return encoding;
        }
    }
    return std::nullopt;
}

auto fbide::encodingNames() -> std::string {
    std::string names;
    for (const auto value : TextEncoding::all) {
        if (!names.empty()) {
            names += ", ";
        }
        names += TextEncoding { value }.toString();
    }
    return names;
}

auto fbide::encodeWithBom(const wxString& text, const TextEncoding encoding) -> std::optional<std::string> {
    const auto encoded = encoding.encode(text);
    if (!encoded.has_value()) {
        return std::nullopt;
    }
    const auto bom = encoding.bomBytes();
    std::string out;
    out.reserve(bom.size() + encoded->length());
    for (const auto byte : bom) {
        out += static_cast<char>(byte);
    }
    out.append(encoded->data(), encoded->length());
    return out;
}
