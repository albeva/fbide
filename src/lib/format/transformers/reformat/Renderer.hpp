//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "FormatTree.hpp"

namespace fbide::reformat {

/// Renders a ProgramTree back to a stream of lexer tokens.
/// Indentation and inter-token spaces are emitted as Whitespace tokens;
/// line breaks as Newline tokens. Significant tokens are copied through
/// with their original kind/keywordKind/operatorKind preserved.
class Renderer final {
public:
    /// Construct with format options pinned for the lifetime of `render`.
    explicit Renderer(const FormatOptions& options)
    : m_options(options) {}

    /// Render the tree to a formatted token stream.
    [[nodiscard]] auto render(const ProgramTree& tree) -> std::vector<lexer::Token>;

private:
    /// Recursively render a node list at the given indent level.
    void renderNodes(const std::vector<Node>& nodes, std::size_t indent);
    /// Render a block (opener, body, closer).
    void renderBlock(const BlockNode& block, std::size_t indent);
    /// Render a single statement at the given indent.
    void renderStatement(const StatementNode& stmt, std::size_t indent);
    /// Render a preprocessor statement in anchored mode (`#` at column 0).
    void renderAnchoredPP(const StatementNode& stmt, std::size_t indent, std::size_t first);
    /// Emit the leading indent for a statement, skipping any leading layout tokens.
    void emitLeadingIndent(const StatementNode& stmt, std::size_t first, std::size_t indent);
    /// Emit `indent × tabSize` spaces as a Whitespace token.
    void emitIndent(std::size_t indent);
    /// Emit a single inter-token space.
    void emitSpace();
    /// Emit a Newline token and reset per-line state.
    void emitNewline();
    /// Append a token to the output verbatim.
    void emit(lexer::Token token);

    /// True when `block` is a branch (Else, Case, `#else`) — opener but no closer.
    [[nodiscard]] static auto isBranch(const BlockNode& block) -> bool;
    /// True when `block` is a top-level definition (Sub, Function, Type).
    [[nodiscard]] static auto isDefinition(const BlockNode& block) -> bool;
    /// True when a space is required between two adjacent tokens.
    [[nodiscard]] static auto needsSpaceBefore(const lexer::Token& prev, const lexer::Token& curr) -> bool;
    /// True when `token` is layout-only (Whitespace, Newline, Comment).
    [[nodiscard]] static auto isLayout(const lexer::Token& token) -> bool;

    FormatOptions m_options;             ///< Format options pinned for this run.
    bool m_lastWasBlankLine = false;     ///< True when the previous emit was a blank line.
    bool m_lastWasBlock = false;         ///< True when the previous emit closed a top-level block.
    std::vector<lexer::Token> m_output;  ///< Output buffer.
};

} // namespace fbide::reformat
