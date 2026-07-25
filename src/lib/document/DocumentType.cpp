//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentType.hpp"

namespace fbide {

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
