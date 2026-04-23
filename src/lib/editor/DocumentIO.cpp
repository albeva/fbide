//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentIO.hpp"
#include <wx/file.h>
#include "EncodingDetector.hpp"
using namespace fbide;

namespace {

/// Convert in-place: any mix of \r\n / \r / \n → the chosen EOL bytes.
auto normalizeEols(const wxString& text, const EolMode mode) -> wxString {
    wxString out;
    out.reserve(text.length());
    const char* eol = nullptr;

    switch (mode.value()) {
    case EolMode::LF:
        eol = "\n";
        break;
    case EolMode::CRLF:
        eol = "\r\n";
        break;
    case EolMode::CR:
        eol = "\r";
        break;
    }

    for (std::size_t i = 0; i < text.length(); i++) {
        const auto ch = text[i];
        if (ch == '\r') {
            out += eol;
            if (i + 1 < text.length() && text[i + 1] == '\n') {
                i++;
            }
        } else if (ch == '\n') {
            out += eol;
        } else {
            out += ch;
        }
    }
    return out;
}

} // namespace

namespace {

/// Read whole file into a byte buffer. Returns nullopt on I/O failure.
auto readAllBytes(const wxString& path) -> std::optional<std::vector<unsigned char>> {
    wxFile file(path, wxFile::read);
    if (!file.IsOpened()) {
        return std::nullopt;
    }

    const auto fileLen = static_cast<std::size_t>(file.Length());
    std::vector<unsigned char> bytes(fileLen);
    if (fileLen > 0 && file.Read(bytes.data(), fileLen) != static_cast<ssize_t>(fileLen)) {
        return std::nullopt;
    }
    return bytes;
}

/// Decode bytes using `encoding`. If that fails, re-decode as ISO-8859-1
/// (1:1 byte-to-codepoint mapping — never rejects input) and report the
/// fallback encoding so the document reflects reality.
struct DecodeResult {
    wxString text;
    TextEncoding encoding;
};
auto decodeOrLatin1(const void* bytes, std::size_t len, TextEncoding encoding) -> DecodeResult {
    if (auto decoded = encoding.decode(bytes, len); decoded.has_value()) {
        return { std::move(*decoded), encoding };
    }
    const TextEncoding latin1 { TextEncoding::ISO_8859_1 };
    auto decoded = latin1.decode(bytes, len);
    return { decoded.value_or(wxString {}), latin1 };
}

/// Strip leading BOM only if present AND matches `encoding`.
auto stripBom(std::span<const unsigned char> bytes, TextEncoding encoding)
    -> std::span<const unsigned char> {
    if (const auto detected = EncodingDetector::detectBom(bytes.data(), bytes.size());
        detected.has_value() && *detected == encoding) {
        return bytes.subspan(encoding.bomLength());
    }
    return bytes;
}

auto decodeAndDetectEol(std::span<const unsigned char> bytes, TextEncoding encoding, EolMode defaultEol)
    -> DocumentIO::LoadResult {
    auto decoded = decodeOrLatin1(bytes.data(), bytes.size(), encoding);
    const auto eol = EncodingDetector::detectEol(decoded.text).value_or(defaultEol);
    return { std::move(decoded.text), decoded.encoding, eol };
}

} // namespace

auto DocumentIO::load(
    const wxString& path,
    const TextEncoding defaultEncoding,
    const EolMode defaultEol
) -> std::optional<LoadResult> {
    const auto bytes = readAllBytes(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const auto detection = EncodingDetector::detect(bytes->data(), bytes->size(), defaultEncoding);
    const auto payload = std::span(*bytes).subspan(detection.hadBom ? detection.encoding.bomLength() : 0);
    return decodeAndDetectEol(payload, detection.encoding, defaultEol);
}

auto DocumentIO::loadWithEncoding(
    const wxString& path,
    const TextEncoding encoding,
    const EolMode defaultEol
) -> std::optional<LoadResult> {
    const auto bytes = readAllBytes(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return decodeAndDetectEol(stripBom(*bytes, encoding), encoding, defaultEol);
}

auto DocumentIO::save(const wxString& path,
    const wxString& text,
    const TextEncoding encoding,
    const EolMode eolMode) -> SaveResult {
    const auto normalized = normalizeEols(text, eolMode);

    const auto encoded = encoding.encode(normalized);
    if (!encoded.has_value()) {
        return SaveResult::EncodingError;
    }

    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) {
        return SaveResult::IOError;
    }

    if (const auto bom = encoding.bomBytes(); !bom.empty()) {
        if (!file.Write(bom.data(), bom.size())) {
            return SaveResult::IOError;
        }
    }

    if (encoded->length() > 0) {
        if (!file.Write(encoded->data(), encoded->length())) {
            return SaveResult::IOError;
        }
    }

    return file.Close() ? SaveResult::Success : SaveResult::IOError;
}
