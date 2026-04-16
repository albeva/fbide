//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "FormatTree.hpp"

namespace fbide::format {

/// Renders a ProgramTree back to formatted source code.
/// Applies indentation based on tree structure and spacing based on OperatorKind.
class Renderer final {
public:
    explicit Renderer(const std::size_t tabSize, const bool anchorHash = false)
        : m_tabSize(tabSize)
        , m_anchorHash(anchorHash) {}

    /// Render the tree to a formatted string.
    [[nodiscard]] auto render(const ProgramTree& tree) -> std::string;

private:
    void renderNodes(const std::vector<Node>& nodes, std::size_t indent);
    void renderBlock(const BlockNode& block, std::size_t indent);
    void renderStatement(const StatementNode& stmt, std::size_t indent);
    void renderAnchoredPP(const StatementNode& stmt, std::size_t indent);
    void emitIndent(std::size_t indent);
    void emitNewline();

    [[nodiscard]] static auto isBranch(const BlockNode& block) -> bool;
    [[nodiscard]] static auto isDefinition(const BlockNode& block) -> bool;
    [[nodiscard]] static auto needsSpaceBefore(const lexer::Token& prev, const lexer::Token& curr) -> bool;

    std::size_t m_tabSize;
    bool m_anchorHash;
    bool m_lastWasBlankLine = false;
    bool m_lastWasBlock = false;
    std::string m_output;
};

} // namespace fbide::format
