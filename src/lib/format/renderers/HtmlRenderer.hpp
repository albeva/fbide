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

/// Renders tokens as HTML with syntax colouring from theme.
class HtmlRenderer final : public Renderer {
public:
    /// If fullDocument is true, wraps output in a complete HTML page.
    /// If false, produces only <code><pre>...</pre></code> tags.
    explicit HtmlRenderer(const Theme& theme, const std::size_t sizeHint)
    : Renderer(sizeHint)
    , m_theme(theme) {}

    /// @copydoc Renderer::render
    [[nodiscard]] auto render(const std::vector<lexer::Token>& tokens) const -> wxString override;
    /// @copydoc Renderer::getType
    [[nodiscard]] auto getType() const -> DocumentType override { return DocumentType::HTML; }
    /// Wrap an inline render in a complete HTML document (head, style, body).
    static auto decorate(const wxString& rendered) -> wxString;

private:
    const Theme& m_theme; ///< Active theme — drives style colour mapping.
};

} // namespace fbide
