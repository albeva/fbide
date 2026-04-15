//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Token.hpp"
namespace fbide {
class Keywords;
}

namespace fbide::lexer {

/// Keyword lookup result: which group and structural role.
struct KeywordInfo {
    TokenKind tokenKind;
    KeywordKind keywordKind;
};

/// Simple FreeBASIC lexer that tokenises source code.
/// No syntax validation — unrecognised input becomes Identifier tokens.
class Lexer final {
public:
    NO_COPY_AND_MOVE(Lexer)

    explicit Lexer(const Keywords& keywords);

    /// Tokenise the given source text.
    [[nodiscard]] auto tokenise(const wxString& source) const -> std::vector<Token>;

private:
    [[nodiscard]] auto classifyWord(const wxString& word) const -> std::pair<TokenKind, KeywordKind>;

    std::unordered_map<wxString, KeywordInfo> m_keywords;

};

} // namespace fbide
