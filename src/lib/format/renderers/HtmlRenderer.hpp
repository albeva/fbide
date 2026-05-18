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
    /// Block wrapper for the rendered code.
    enum class Wrap : std::uint8_t {
        Pre,   ///< `<pre>` with a CSS background — browsers / full CSS.
        Table, ///< `<table bgcolor>` around the `<pre>` — for wxHtmlWindow,
               ///< which honours table `bgcolor` but not block backgrounds.
    };

    /// Construct with the theme, a size hint, and the block wrapper mode.
    explicit HtmlRenderer(const Theme& theme, const std::size_t sizeHint, const Wrap wrap = Wrap::Pre)
    : Renderer(sizeHint)
    , m_theme(theme)
    , m_wrap(wrap) {}

    /// @copydoc Renderer::render
    [[nodiscard]] auto render(const std::vector<lexer::Token>& tokens) const -> wxString override;
    /// @copydoc Renderer::getType
    [[nodiscard]] auto getType() const -> DocumentType override { return DocumentType::HTML; }
    /// Wrap an inline render in a complete HTML document (head, style, body).
    static auto decorate(const wxString& rendered) -> wxString;

private:
    const Theme& m_theme; ///< Active theme — drives style colour mapping.
    Wrap m_wrap;          ///< Block wrapper mode.
};

} // namespace fbide
