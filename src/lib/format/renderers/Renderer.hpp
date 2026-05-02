//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"
#include "document/DocumentType.hpp"
#include "format/transformers/Transform.hpp"

namespace fbide {

/// Abstract base for token renderers. Terminal step in a format pipeline.
class Renderer {
public:
    NO_COPY_AND_MOVE(Renderer)
    /// Construct with a size hint to pre-reserve the output buffer.
    explicit Renderer(const std::size_t sizeHint)
    : m_sizeHint(sizeHint) {}
    virtual ~Renderer() = default;

    /// Document type the renderer produces.
    [[nodiscard]] virtual auto getType() const -> DocumentType = 0;

    /// Render the (already transformed) tokens to a `wxString`.
    [[nodiscard]] virtual auto render(const std::vector<lexer::Token>& tokens) const -> wxString = 0;

    /// Original input size — used by subclasses to pre-reserve buffers.
    [[nodiscard]] auto getSizeHint() const -> std::size_t { return m_sizeHint; }

private:
    std::size_t m_sizeHint; ///< Cached size hint passed at construction.
};

} // namespace fbide
