//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EncodingDetector.hpp"
#include <wx/convauto.h>
using namespace fbide;

namespace {

/// Max bytes to scan when counting EOL markers.
constexpr std::size_t kEolScanLimit = 8 * 1024;

} // namespace

auto EncodingDetector::detect(const void* bytes, const std::size_t len, const TextEncoding fallback) -> DetectionResult {
    if (const auto fromBom = detectBom(bytes, len); fromBom.has_value()) {
        return { *fromBom, true };
    }
    if (isLikelyUtf8(bytes, len)) {
        return { TextEncoding { TextEncoding::UTF8 }, false };
    }
    // Bytes are not valid UTF-8 but the caller's fallback is UTF-*.
    // Decoding would fail; substitute the system default so legacy files
    // (e.g. Windows-1252 .fbl, CP437 DOS sources) still load.
    if (!isValidUtf8(bytes, len)) {
        switch (fallback.value()) {
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM:
        case TextEncoding::UTF16_LE:
        case TextEncoding::UTF16_BE:
            return { TextEncoding { TextEncoding::System }, false };
        default:
            break;
        }
    }
    return { fallback, false };
}

auto EncodingDetector::detectBom(const void* bytes, const std::size_t len) -> std::optional<TextEncoding> {
    if (bytes == nullptr || len == 0) {
        return std::nullopt;
    }
    const auto bom = wxConvAuto::DetectBOM(static_cast<const char*>(bytes), len);
    return TextEncoding::fromWxBom(bom);
}

auto EncodingDetector::isValidUtf8(const void* bytes, const std::size_t len) -> bool {
    if (len == 0) {
        return true;
    }
    wxMBConvUTF8 conv;
    const size_t wideLen = conv.ToWChar(nullptr, 0, static_cast<const char*>(bytes), len);
    return wideLen != wxCONV_FAILED;
}

auto EncodingDetector::isLikelyUtf8(const void* bytes, const std::size_t len) -> bool {
    if (!isValidUtf8(bytes, len)) {
        return false;
    }
    const auto* data = static_cast<const unsigned char*>(bytes);
    for (std::size_t i = 0; i < len; i++) {
        if (data[i] >= 0x80) {
            return true;
        }
    }
    return false;
}

auto EncodingDetector::detectEol(const wxString& text) -> std::optional<EolMode> {
    std::size_t lf = 0;
    std::size_t crlf = 0;
    std::size_t cr = 0;

    const auto limit = std::min(text.length(), kEolScanLimit);
    for (std::size_t i = 0; i < limit; i++) {
        const auto ch = text[i];
        if (ch == '\r') {
            if (i + 1 < limit && text[i + 1] == '\n') {
                crlf++;
                i++;
            } else {
                cr++;
            }
        } else if (ch == '\n') {
            lf++;
        }
    }

    if (lf == 0 && crlf == 0 && cr == 0) {
        return std::nullopt;
    }

    // Strict winner — ties return nullopt so caller can fall back to config default
    if (crlf > lf && crlf > cr) {
        return EolMode { EolMode::CRLF };
    }
    if (lf > crlf && lf > cr) {
        return EolMode { EolMode::LF };
    }
    if (cr > crlf && cr > lf) {
        return EolMode { EolMode::CR };
    }
    return std::nullopt;
}
