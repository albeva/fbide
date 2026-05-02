//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"

namespace fbide::reformat {

/// Formatting options.
struct FormatOptions {
    std::size_t tabSize = 4;  ///< Indent width in spaces.
    bool anchoredPP = false;  ///< When true, preprocessor directives anchor at column 0.
    bool reIndent = true;     ///< Apply structural indentation.
    bool reFormat = true;     ///< Apply inter-token spacing + blank-line policy.
    /// Build a lean tree: drop Whitespace, Comment, CommentBlock tokens and
    /// collapse runs of Newlines into a single separator. Intended for
    /// non-rendering consumers (sub/function browser, future analyses).
    /// Tokens inside `' format off` (verbatim) regions pass through untouched.
    bool lean = false;
};

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

/// A node in the formatting tree.
using Node = std::variant<BlankLineNode, StatementNode, VerbatimNode, std::unique_ptr<BlockNode>>;

/// A block: optional opener, body of child nodes, optional closer.
/// Branches (Else, Case, `#else`) are child BlockNodes with an opener but no closer.
struct BlockNode {
    std::optional<StatementNode> opener; ///< Block opener (e.g. `Sub Foo()`).
    std::vector<Node> body;              ///< Child nodes.
    std::optional<StatementNode> closer; ///< Block closer (e.g. `End Sub`). Empty for branches.
};

/// Root of the formatting tree.
struct ProgramTree {
    std::vector<Node> nodes; ///< Top-level node list.
};

} // namespace fbide::reformat
