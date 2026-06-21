//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"

namespace fbide::parser {

/// Blank line preserved from source.
struct BlankLineNode {};

/// A single logical statement — flat sequence of tokens.
struct StatementNode {
    std::vector<lexer::Token> tokens; ///< Tokens making up the statement.
};

/// A contiguous run of tokens from a `' format off` region. Rendered as-is
/// (original text, original whitespace, no indent prefix, no space
/// normalisation). Bypasses all structural formatting.
struct VerbatimNode {
    std::vector<lexer::Token> tokens; ///< Tokens emitted verbatim, including layout.
};

/// Forward declare for recursive variant.
struct BlockNode;

/// A node in the parse tree.
using Node = std::variant<BlankLineNode, StatementNode, VerbatimNode, std::unique_ptr<BlockNode>>;

/// A block: optional opener, body of child nodes, optional closer.
/// Branches (Else, Case, `#else`) are child BlockNodes with an opener but no closer.
struct BlockNode {
    std::optional<StatementNode> opener; ///< Block opener (e.g. `Sub Foo()`).
    std::vector<Node> body;              ///< Child nodes.
    std::optional<StatementNode> closer; ///< Block closer (e.g. `End Sub`). Empty for branches.
    /// Enclosing block, or null at the top level. Wired by `TreeBuilder`
    /// as each block is attached to its parent, so analyses can walk up
    /// the scope chain. Not set by hand-built trees. Heap-stable (the
    /// pointee lives behind a `unique_ptr`), so it survives moving the tree.
    BlockNode* parent = nullptr;
};

/// Root of the parse tree.
struct ProgramTree {
    std::vector<Node> nodes; ///< Top-level node list.
};

} // namespace fbide::parser
