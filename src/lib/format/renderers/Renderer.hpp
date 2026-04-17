//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/analyses/lexer/Token.hpp"
#include "lib/format/formatters/TokenTransform.hpp"
#include "lib/editor/DocumentType.hpp"

namespace fbide {

/// Abstract base for token renderers. Terminal step in a format pipeline.
class Renderer {
public:
    NO_COPY_AND_MOVE(Renderer)
    explicit Renderer(const std::size_t sizeHint) : m_sizeHint(sizeHint) {}
    virtual ~Renderer() = default;

    /// If the renderer produces a new document, return its type.
    /// std::nullopt means in-place edit of the active document.
    [[nodiscard]] virtual auto getType() const -> DocumentType = 0;

    /// Subclasses implement this to render the (already transformed) tokens.
    [[nodiscard]] virtual auto render(const std::vector<lexer::Token>& tokens) const -> wxString = 0;

    /// Get input size hint
    [[nodiscard]] auto getSizeHint() const -> std::size_t { return m_sizeHint; }
private:
    std::size_t m_sizeHint;
};

} // namespace fbide
