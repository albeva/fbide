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
#include "lib/format/transforms/Transform.hpp"

namespace fbide::reformat {

/// Reformat transform: reads a lexer token stream, scans it into a
/// ProgramTree, and renders the tree back as a formatted token stream.
/// Composable in a TokenTransform pipeline.
///
/// Reusable: calling apply() (or buildTree()) multiple times resets the
/// per-invocation scan state at entry.
class ReFormatter final : public Transform {
public:
    explicit ReFormatter(const FormatOptions& options) : m_options(options) {}

    [[nodiscard]] auto apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> override;

    /// Scan a token stream into a ProgramTree. Exposed for testing the
    /// scan stage in isolation.
    [[nodiscard]] auto buildTree(const std::vector<lexer::Token>& tokens) -> ProgramTree;

private:
    // Navigation
    [[nodiscard]] auto hasMore() const -> bool { return m_index < m_tokens->size(); }
    [[nodiscard]] auto current() const -> const lexer::Token& { return (*m_tokens)[m_index]; }
    void advance() { m_index++; }

    void step();
    void processLine();
    [[nodiscard]] auto isContinuation() const -> bool;

    void dispatch();
    void openBlockOrStatement();
    [[nodiscard]] auto firstKeyword() const -> lexer::KeywordKind;
    [[nodiscard]] auto lastSignificantKeyword() const -> lexer::KeywordKind;
    [[nodiscard]] auto isBodyDefinition() const -> bool;
    [[nodiscard]] auto hasBlockCloserAfterFirst() const -> bool;

    FormatOptions m_options;

    // Per-invocation scan state (reset by buildTree).
    const std::vector<lexer::Token>* m_tokens = nullptr;
    std::size_t m_index = 0;
    bool m_prevWasNewline = true;
    TreeBuilder m_builder;
    std::vector<lexer::Token> m_segment;
    std::vector<std::size_t> m_ppDepths; // builder stack depth at each PP block open
};

} // namespace fbide::format
