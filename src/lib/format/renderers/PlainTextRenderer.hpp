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
    [[nodiscard]] auto render(const std::vector<lexer::Token>& tokens) const -> wxString override;
    [[nodiscard]] auto getType() const -> DocumentType override { return DocumentType::FreeBASIC; }
};

} // namespace fbide
