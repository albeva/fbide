//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "FormatTree.hpp"
#include "TreeBuilder.hpp"
#include "format/transformers/Transform.hpp"

namespace fbide::reformat {

/// Reformat transform: reads a lexer token stream, scans it into a
/// ProgramTree, and renders the tree back as a formatted token stream.
/// Composable in a TokenTransform pipeline.
///
/// Reusable: calling apply() (or buildTree()) multiple times resets the
/// per-invocation scan state at entry.
class ReFormatter final : public Transform {
public:
    /// Construct with format options pinned for the lifetime of `apply`.
    explicit ReFormatter(const FormatOptions& options)
    : m_options(options) {}

    /// Apply the reformat — tokens in, formatted tokens out.
    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> override;

    /// Scan a token stream into a ProgramTree. Exposed for testing the
    /// scan stage in isolation.
    [[nodiscard]] auto buildTree(const std::vector<lexer::Token>& tokens) -> ProgramTree;

private:
    // Navigation
    /// True while the scan position has more tokens to consume.
    [[nodiscard]] auto hasMore() const -> bool { return m_index < m_tokens->size(); }
    /// The token at the current scan position.
    [[nodiscard]] auto current() const -> const lexer::Token& { return (*m_tokens)[m_index]; }
    /// Step the scan position forward by one token.
    void advance() { m_index++; }

    /// Single dispatch step in the scan loop.
    void step();
    /// Process the in-progress line segment now that a newline was seen.
    void processLine();

    /// Classify the buffered segment and forward it to `TreeBuilder`.
    void dispatch();
    /// Treat the buffered segment as a block opener or a statement.
    void openBlockOrStatement();
    /// First significant keyword in the buffered segment.
    [[nodiscard]] auto firstKeyword() const -> lexer::KeywordKind;
    /// Last significant keyword in the buffered segment.
    [[nodiscard]] auto lastSignificantKeyword() const -> lexer::KeywordKind;
    /// True when the segment opens a body definition (`Sub`, `Function`, ...).
    [[nodiscard]] auto isBodyDefinition() const -> bool;
    /// True when the segment contains a block closer after the first keyword.
    [[nodiscard]] auto hasBlockCloserAfterFirst() const -> bool;

    FormatOptions m_options; ///< Format options pinned for this run.

    // Per-invocation scan state (reset by buildTree).
    const std::vector<lexer::Token>* m_tokens = nullptr; ///< Active token stream (filtered or original).
    std::size_t m_index = 0;             ///< Current scan index into `*m_tokens`.
    bool m_prevWasNewline = true;        ///< True when the prior token was a newline / start of input.
    TreeBuilder m_builder;               ///< Tree being assembled.
    std::vector<lexer::Token> m_segment; ///< Buffered tokens for the current statement / opener.
    std::vector<std::size_t> m_ppDepths; ///< Builder stack depth recorded at each PP block open.
};

} // namespace fbide::reformat
