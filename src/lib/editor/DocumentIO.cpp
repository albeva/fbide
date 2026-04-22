//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentIO.hpp"
#include "EncodingDetector.hpp"
#include <wx/file.h>
using namespace fbide;

namespace {

/// Convert in-place: any mix of \r\n / \r / \n → the chosen EOL bytes.
auto normalizeEols(const wxString& text, const EolMode mode) -> wxString {
    wxString out;
    out.reserve(text.length());
    const char* eol = nullptr;
    switch (mode.value()) {
    case EolMode::LF:   eol = "\n";   break;
    case EolMode::CRLF: eol = "\r\n"; break;
    case EolMode::CR:   eol = "\r";   break;
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

auto DocumentIO::load(const wxString& path,
    const TextEncoding defaultEncoding,
    const EolMode defaultEol) -> std::optional<LoadResult> {
    wxFile file(path, wxFile::read);
    if (!file.IsOpened()) {
        return std::nullopt;
    }

    const auto fileLen = static_cast<std::size_t>(file.Length());
    std::vector<unsigned char> bytes(fileLen);
    if (fileLen > 0) {
        if (file.Read(bytes.data(), fileLen) != static_cast<ssize_t>(fileLen)) {
            return std::nullopt;
        }
    }

    const auto detection = EncodingDetector::detect(bytes.data(), fileLen, defaultEncoding);

    // Strip BOM prefix if present before decoding. BOM length is known
    // from the encoding — saves an extra sniff.
    const auto bomLen = detection.hadBom ? detection.encoding.bomLength() : 0;
    const void* payload = bytes.data() + bomLen;
    const auto payloadLen = fileLen - bomLen;

    auto decoded = detection.encoding.decode(payload, payloadLen);
    if (!decoded.has_value()) {
        return std::nullopt;
    }

    const auto detectedEol = EncodingDetector::detectEol(*decoded).value_or(defaultEol);

    return LoadResult {
        std::move(*decoded),
        detection.encoding,
        detectedEol,
    };
}

auto DocumentIO::save(const wxString& path,
    const wxString& text,
    const TextEncoding encoding,
    const EolMode eolMode) -> bool {
    const auto normalized = normalizeEols(text, eolMode);

    const auto encoded = encoding.encode(normalized);
    if (!encoded.has_value()) {
        return false;
    }

    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) {
        return false;
    }

    // Prefix BOM if the chosen encoding mandates one.
    if (const auto bom = encoding.bomBytes(); !bom.empty()) {
        if (!file.Write(bom.data(), bom.size())) {
            return false;
        }
    }

    if (encoded->length() > 0) {
        if (!file.Write(encoded->data(), encoded->length())) {
            return false;
        }
    }

    return file.Close();
}
