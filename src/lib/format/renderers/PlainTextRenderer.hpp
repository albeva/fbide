//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Renderer.hpp"

namespace fbide {

/// Renders tokens back to plain text by concatenating token text.
class PlainTextRenderer final : public Renderer {
public:
    using Renderer::Renderer;
    [[nodiscard]] auto render(const std::vector<lexer::Token>& tokens) const -> wxString override;
    /// The concatenated token text as raw UTF-8 bytes — what `render` decodes.
    /// Lets callers validate the bytes (see `isSafeFormatResult`) before
    /// `wxString::FromUTF8` would turn invalid input into an empty string.
    [[nodiscard]] auto renderUtf8(const std::vector<lexer::Token>& tokens) const -> std::string;
    [[nodiscard]] auto getType() const -> DocumentType override { return DocumentType::FreeBASIC; }
};

} // namespace fbide
