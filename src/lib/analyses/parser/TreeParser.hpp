//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ProgramTree.hpp"
#include "TreeBuilder.hpp"

namespace fbide::parser {

/// Options controlling how a token stream is scanned into a tree.
struct ParseOptions {
    /// Build a lean tree: drop Whitespace, Comment, CommentBlock tokens and
    /// collapse runs of Newlines into a single separator. Intended for
    /// non-rendering consumers (symbol browser, scope/keyword matching).
    /// Tokens inside `' format off` (verbatim) regions pass through untouched.
    bool lean = false;
    /// Split statements on `:` and treat a single physical line that both
    /// opens and closes a block as one self-contained statement. When false,
    /// the original inline layout is preserved for verbatim-style rendering.
    bool reFormat = true;
};

/// Scans a lexer token stream into a `ProgramTree`, driving a `TreeBuilder`.
///
/// Reusable: every `parse()` resets the per-invocation scan state at entry,
/// so a single instance can be kept alive and fed successive token streams —
/// the builder's node / token buffers are recycled across calls.
class TreeParser final {
public:
    /// Construct with parse options pinned for the lifetime of the parser.
    explicit TreeParser(const ParseOptions& options)
    : m_options(options) {}

    /// Scan `tokens` into a ProgramTree. `recycle` (when non-empty) seeds the
    /// builder's BlockNode free-list so the previous parse's nodes are reused
    /// instead of reallocated.
    [[nodiscard]] auto parse(const std::vector<lexer::Token>& tokens, ProgramTree&& recycle = {}) -> ProgramTree;

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

    ParseOptions m_options; ///< Parse options pinned for this parser.

    // Per-invocation scan state (reset by parse).
    const std::vector<lexer::Token>* m_tokens = nullptr; ///< Active token stream (filtered or original).
    std::size_t m_index = 0;                             ///< Current scan index into `*m_tokens`.
    bool m_prevWasNewline = true;                        ///< True when the prior token was a newline / start of input.
    TreeBuilder m_builder;                               ///< Tree being assembled.
    std::vector<lexer::Token> m_segment;                 ///< Buffered tokens for the current statement / opener.
    std::vector<lexer::Token> m_lean;                    ///< Reusable lean-filtered token buffer; avoids a per-build allocation.
    std::vector<std::size_t> m_ppDepths;                 ///< Builder stack depth recorded at each PP block open.
};

} // namespace fbide::parser
