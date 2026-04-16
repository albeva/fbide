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

/// Stack-based tree builder driven by the scanner.
///
/// The scanner iterates lexer tokens and calls these methods to
/// construct the formatting tree. The builder maintains a stack of
/// open BlockNodes and a token collection buffer.
class TreeBuilder final {
public:
    /// Add a token to the collection buffer.
    void append(const lexer::Token& token);

    /// Flush buffer as a StatementNode in the current block's body.
    void statement();

    /// Flush buffer as the opener of a new BlockNode. Pushes onto stack.
    void openBlock();

    /// Close the current branch (if any), then flush buffer as the opener
    /// of a new branch BlockNode. Branches have no closer — they are closed
    /// implicitly by the next branch or by closeBlock().
    void openBranch();

    /// Close any open branch, then flush buffer as the closer of the
    /// current block. Pops the block and adds it to the parent.
    /// If the stack is empty (unmatched closer), emits as a statement.
    void closeBlock();

    /// Add a BlankLineNode to the current block's body.
    void blankLine();

    /// Auto-close any unclosed blocks and return the root ProgramTree.
    [[nodiscard]] auto finish() -> ProgramTree;

    /// Current stack depth (for PP boundary tracking).
    [[nodiscard]] auto stackDepth() const -> std::size_t { return m_stack.size(); }

    /// Auto-close blocks until the stack reaches the given depth.
    /// Used when PP boundaries cut across open code blocks.
    void closeToDepth(std::size_t depth);

private:
    void addNode(Node node);
    void closeBranch();
    [[nodiscard]] auto flushTokens() -> StatementNode;

    struct StackEntry {
        std::unique_ptr<BlockNode> node;
        bool isBranch = false;
    };

    std::vector<lexer::Token> m_collected;
    std::vector<StackEntry> m_stack;
    std::vector<Node> m_root;
};

} // namespace fbide::format
