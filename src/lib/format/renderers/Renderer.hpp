//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/format/Token.hpp"
#include "lib/format/formatters/TokenTransform.hpp"
#include "lib/editor/DocumentType.hpp"

namespace fbide {

/// Abstract base for token renderers. Terminal step in a format pipeline.
class Renderer {
public:
    virtual ~Renderer() = default;

    /// Render the token stream to a string, applying transforms first.
    [[nodiscard]] auto render(
        const std::vector<Token>& tokens,
        const std::vector<std::unique_ptr<TokenTransform>>& transforms
    ) const -> wxString;

    /// If the renderer produces a new document, return its type.
    /// std::nullopt means in-place edit of the active document.
    [[nodiscard]] virtual auto getType() const -> DocumentType = 0;

    /// Subclasses implement this to render the (already transformed) tokens.
    [[nodiscard]] virtual auto render(const std::vector<Token>& tokens) const -> wxString = 0;
};

} // namespace fbide
