//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/analyses/lexer/Token.hpp"

namespace fbide::reformat {

/// Formatting options.
struct FormatOptions {
    std::size_t tabSize = 4;
    bool anchoredPP = false;
    bool reIndent = true;
    bool reFormat = true;
};

/// Blank line preserved from source.
struct BlankLineNode {};

/// A single logical statement — flat sequence of tokens.
struct StatementNode {
    std::vector<lexer::Token> tokens;
};

/// Forward declare for recursive variant.
struct BlockNode;

/// A node in the formatting tree.
using Node = std::variant<BlankLineNode, StatementNode, std::unique_ptr<BlockNode>>;

/// A block: optional opener, body of child nodes, optional closer.
/// Branches (Else, Case, #else) are child BlockNodes with an opener but no closer.
struct BlockNode {
    std::optional<StatementNode> opener;
    std::vector<Node> body;
    std::optional<StatementNode> closer;
};

/// Root of the formatting tree.
struct ProgramTree {
    std::vector<Node> nodes;
};

/// Helper for std::visit with multiple lambdas.
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

} // namespace fbide::format
