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
class Theme;

/// Renders tokens as BBCode with syntax colouring from theme.
class BBCodeRenderer final : public Renderer {
public:
    explicit BBCodeRenderer(const Theme& theme) : m_theme(theme) {}

    [[nodiscard]] auto render(const std::vector<lexer::Token>& tokens) const -> wxString override;
    [[nodiscard]] auto getType() const -> DocumentType override { return DocumentType::Text; }

private:
    const Theme& m_theme;
};

} // namespace fbide
