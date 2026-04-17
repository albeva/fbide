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

namespace fbide::format {

/// Scans a lexer token stream and builds a ProgramTree by driving TreeBuilder.
/// Handles whitespace skipping, colon splitting, line continuation, blank lines,
/// and keyword/PP dispatch.
class Scanner final {
public:
    /// Scan the token stream and return the formatting tree.
    [[nodiscard]] static auto scan(const std::vector<lexer::Token>& tokens, const FormatOptions& options) -> ProgramTree;

private:
    Scanner(const std::vector<lexer::Token>& tokens, const FormatOptions& options);
    void run();

    // Navigation
    [[nodiscard]] auto hasMore() const -> bool { return m_index < m_tokens.size(); }
    [[nodiscard]] auto current() const -> const lexer::Token& { return m_tokens[m_index]; }
    void advance() { m_index++; }

    // Line processing
    void processLine();
    [[nodiscard]] auto isContinuation() const -> bool;

    // Dispatch collected segment to builder
    void dispatch();
    void openBlockOrStatement();
    [[nodiscard]] auto firstKeyword() const -> lexer::KeywordKind;
    [[nodiscard]] auto lastSignificantKeyword() const -> lexer::KeywordKind;
    [[nodiscard]] auto isBodyDefinition() const -> bool;
    [[nodiscard]] auto hasBlockCloserAfterFirst() const -> bool;

    const std::vector<lexer::Token>& m_tokens;
    const FormatOptions& m_options;
    std::size_t m_index = 0;
    bool m_prevWasNewline = true;

    TreeBuilder m_builder;
    std::vector<lexer::Token> m_segment;
    std::vector<std::size_t> m_ppDepths; // builder stack depth at each PP block open
};

} // namespace fbide::format
