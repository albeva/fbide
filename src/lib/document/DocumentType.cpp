//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentType.hpp"

namespace fbide {

namespace {
    auto toLowerAscii(std::string s) -> std::string {
        std::ranges::transform(s, s.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return s;
    }
} // namespace

auto documentTypeFromPath(const std::filesystem::path& path) -> DocumentType {
    // Filename match first — Makefiles typically have no extension.
    // Case-insensitive on macOS/Windows naturally, but normalise for POSIX.
    const auto filename = toLowerAscii(path.filename().string());
    if (filename == "makefile" || filename == "gnumakefile") {
        return DocumentType::Makefile;
    }

    // Extension match. fs::path::extension() returns including the leading dot, e.g. ".bas".
    auto ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(0, 1);
    }
    ext = toLowerAscii(std::move(ext));

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
    if (ext == "bat" || ext == "cmd") {
        return DocumentType::Batch;
    }
    if (ext == "sh" || ext == "bash") {
        return DocumentType::Bash;
    }
    if (ext == "mk" || ext == "make") {
        return DocumentType::Makefile;
    }
    if (ext == "json" || ext == "json5") {
        return DocumentType::Json;
    }
    if (ext == "css") {
        return DocumentType::Css;
    }
    return DocumentType::Text;
}

auto documentTypeKey(const DocumentType type) -> std::string_view {
    switch (type) {
    case DocumentType::FreeBASIC:
        return "freebasic";
    case DocumentType::HTML:
        return "html";
    case DocumentType::Properties:
        return "properties";
    case DocumentType::Markdown:
        return "markdown";
    case DocumentType::Batch:
        return "batch";
    case DocumentType::Bash:
        return "bash";
    case DocumentType::Makefile:
        return "makefile";
    case DocumentType::Json:
        return "json";
    case DocumentType::Css:
        return "css";
    case DocumentType::Text:
        return "text";
    }
    std::unreachable();
}

auto documentTypeFromKey(const std::string_view key) -> std::optional<DocumentType> {
    if (key == "freebasic")
        return DocumentType::FreeBASIC;
    if (key == "html")
        return DocumentType::HTML;
    if (key == "properties")
        return DocumentType::Properties;
    if (key == "markdown")
        return DocumentType::Markdown;
    if (key == "batch")
        return DocumentType::Batch;
    if (key == "bash")
        return DocumentType::Bash;
    if (key == "makefile")
        return DocumentType::Makefile;
    if (key == "json")
        return DocumentType::Json;
    if (key == "css")
        return DocumentType::Css;
    if (key == "text")
        return DocumentType::Text;
    return std::nullopt;
}

} // namespace fbide
