//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentType.hpp"

namespace fbide {

auto documentTypeFromPath(const std::filesystem::path& path) -> DocumentType {
    // fs::path::extension() returns including the leading dot, e.g. ".bas".
    auto ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(0, 1);
    }
    std::ranges::transform(ext, ext.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (ext == "bas" || ext == "bi") {
        return DocumentType::FreeBASIC;
    }
    if (ext == "html" || ext == "htm") {
        return DocumentType::HTML;
    }
    if (ext == "ini" || ext == "fbt" || ext == "lng" || ext == "fbl") {
        return DocumentType::Properties;
    }
    if (ext == "md" || ext == "markdown") {
        return DocumentType::Markdown;
    }
    return DocumentType::Text;
}

} // namespace fbide
