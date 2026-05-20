//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Document type determines lexer and syntax highlighting behaviour.
enum class DocumentType {
    FreeBASIC,  // .bas, .bi — FreeBASIC lexer with keyword groups
    HTML,       // .html, .htm — HTML lexer
    Properties, // .ini, old fbide config files
    Markdown,   // .md, .markdown — Scintilla Markdown lexer
    Text,       // .txt and anything else — no lexer
};

/// Determine document type from file path extension.
[[nodiscard]] auto documentTypeFromPath(const std::filesystem::path& path) -> DocumentType;

} // namespace fbide
