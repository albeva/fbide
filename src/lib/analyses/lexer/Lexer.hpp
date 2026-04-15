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
struct TokenInfo {
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
    [[nodiscard]] auto tokenise(const wxString& source) -> std::vector<Token>;

private:
    // Cursor helpers
    [[nodiscard]] auto current() const -> wxUniChar;
    [[nodiscard]] auto peek() const -> wxUniChar;
    [[nodiscard]] auto atEnd() const -> bool;
    void advance(unsigned count = 1);
    void skipWhile(bool (*pred)(wxUniChar));
    void skipToLineEnd();
    [[nodiscard]] auto extract() const -> wxString;
    [[nodiscard]] auto makeToken(TokenKind kind, KeywordKind kwKind = KeywordKind::None) const -> Token;

    // String lexing modes
    enum class StringMode { Normal, Escaped };

    // Token producers
    [[nodiscard]] auto next() -> Token;
    [[nodiscard]] auto newline() -> Token;
    [[nodiscard]] auto whitespace() -> Token;
    [[nodiscard]] auto comment() -> Token;
    [[nodiscard]] auto commentBlock() -> Token;
    [[nodiscard]] auto stringLiteral(StringMode mode) -> Token;
    [[nodiscard]] auto preprocessor() -> Token;
    [[nodiscard]] auto number() -> Token;
    [[nodiscard]] auto identifier() -> Token;

    // Keyword classification
    [[nodiscard]] auto classifyWord(const wxString& text) const -> TokenInfo;

    // Keyword lookup table (built once)
    std::unordered_map<wxString, TokenInfo> m_keywords;

    // Scanning state (per tokenise() call)
    const wxString* m_source = nullptr;
    unsigned m_len = 0;
    unsigned m_pos = 0;
    unsigned m_start = 0;
    bool m_atLineStart = true;
};

} // namespace fbide
