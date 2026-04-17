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
    explicit Renderer(const FormatOptions& options)
        : m_options(options) {}

    /// Render the tree to a formatted token stream.
    [[nodiscard]] auto render(const ProgramTree& tree) -> std::vector<lexer::Token>;

private:
    void renderNodes(const std::vector<Node>& nodes, std::size_t indent);
    void renderBlock(const BlockNode& block, std::size_t indent);
    void renderStatement(const StatementNode& stmt, std::size_t indent);
    void renderAnchoredPP(const StatementNode& stmt, std::size_t indent, std::size_t first);
    void emitLeadingIndent(const StatementNode& stmt, std::size_t first, std::size_t indent);
    void emitIndent(std::size_t indent);
    void emitSpace();
    void emitNewline();
    void emit(lexer::Token token);

    [[nodiscard]] static auto isBranch(const BlockNode& block) -> bool;
    [[nodiscard]] static auto isDefinition(const BlockNode& block) -> bool;
    [[nodiscard]] static auto needsSpaceBefore(const lexer::Token& prev, const lexer::Token& curr) -> bool;
    [[nodiscard]] static auto isLayout(const lexer::Token& token) -> bool;

    FormatOptions m_options;
    bool m_lastWasBlankLine = false;
    bool m_lastWasBlock = false;
    std::vector<lexer::Token> m_output;
};

} // namespace fbide::format
