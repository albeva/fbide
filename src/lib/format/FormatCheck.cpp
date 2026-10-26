//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatCheck.hpp"
using namespace fbide;

namespace {

auto isLayoutByte(const char ch) -> bool {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

auto foldAscii(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

/// Strict UTF-8 structure check: lead byte, continuation count, no overlongs
/// or surrogates — the same input `wxString::FromUTF8` would reject.
auto isValidUtf8(const std::string_view text) -> bool {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        char32_t code = 0;
        if (lead < 0x80) {
            i++;
            continue;
        }
        if ((lead & 0xE0) == 0xC0) {
            extra = 1;
            code = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            extra = 2;
            code = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            extra = 3;
            code = lead & 0x07;
        } else {
            return false;
        }
        if (i + extra >= text.size()) {
            return false; // truncated sequence
        }
        for (std::size_t k = 1; k <= extra; k++) {
            const auto cont = static_cast<unsigned char>(text[i + k]);
            if ((cont & 0xC0) != 0x80) {
                return false;
            }
            code = (code << 6) | (cont & 0x3F);
        }
        constexpr std::array<char32_t, 4> kMinForLength { 0, 0x80, 0x800, 0x10000 };
        if (code < kMinForLength[extra] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

} // namespace

auto fbide::isSafeFormatResult(const std::string_view source, const std::string_view formatted) -> bool {
    if (!isValidUtf8(formatted)) {
        return false;
    }

    std::size_t src = 0;
    std::size_t out = 0;
    while (true) {
        while (src < source.size() && isLayoutByte(source[src])) {
            src++;
        }
        while (out < formatted.size() && isLayoutByte(formatted[out])) {
            out++;
        }
        if (src == source.size() || out == formatted.size()) {
            break;
        }
        if (foldAscii(source[src]) == foldAscii(formatted[out])) {
            src++;
            out++;
            continue;
        }
        // A `:` separator the formatter split into a line break is dropped
        // from the output — the only removal allowed.
        if (source[src] == ':') {
            src++;
            continue;
        }
        return false;
    }

    // Whatever is left must be layout or dropped separators.
    for (; src < source.size(); src++) {
        if (!isLayoutByte(source[src]) && source[src] != ':') {
            return false;
        }
    }
    for (; out < formatted.size(); out++) {
        if (!isLayoutByte(formatted[out])) {
            return false;
        }
    }
    return true;
}
