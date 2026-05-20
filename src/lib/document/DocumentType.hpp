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
    Batch,      // .bat, .cmd — Scintilla Batch lexer
    Bash,       // .sh, .bash — Scintilla Bash lexer
    Makefile,   // Makefile / .mk / .make — Scintilla Makefile lexer
    Json,       // .json, .json5 — Scintilla JSON lexer
    Text,       // .txt and anything else — no lexer
};

/// Determine document type from file path extension.
[[nodiscard]] auto documentTypeFromPath(const std::filesystem::path& path) -> DocumentType;

/// Stable string key for a document type — used for session serialization
/// and as a locale lookup key (`statusbar/type/<key>`). The set of keys is
/// part of the on-disk session format; do not rename them lightly.
[[nodiscard]] auto documentTypeKey(DocumentType type) -> std::string_view;

/// Inverse of `documentTypeKey`. Returns nullopt for unknown keys —
/// callers fall back to the path-derived type.
[[nodiscard]] auto documentTypeFromKey(std::string_view key) -> std::optional<DocumentType>;

/// Enumeration of every document type in stable order. Used by the
/// status-bar type menu to populate items.
inline constexpr std::array kDocumentTypes {
    DocumentType::FreeBASIC,
    DocumentType::HTML,
    DocumentType::Properties,
    DocumentType::Markdown,
    DocumentType::Batch,
    DocumentType::Bash,
    DocumentType::Makefile,
    DocumentType::Json,
    DocumentType::Text,
};

} // namespace fbide
