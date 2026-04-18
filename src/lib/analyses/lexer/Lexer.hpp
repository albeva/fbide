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
/// Operates on null-terminated UTF-8 input. Keywords, numbers, and operators
/// are ASCII; UTF-8 multi-byte sequences are valid in identifiers, comments,
/// and string literals.
class Lexer final {
public:
    NO_COPY_AND_MOVE(Lexer)

    explicit Lexer(const Keywords& keywords);

    /// Tokenise the given null-terminated UTF-8 source.
    [[nodiscard]] auto tokenise(const char* source) -> std::vector<Token>;

private:
    // String lexing modes
    enum class StringMode { Normal,
        Escaped };

    // Cursor helpers
    [[nodiscard]] auto current() const -> char { return *m_pos; }
    [[nodiscard]] auto peek() const -> char { return m_pos[1]; }
    [[nodiscard]] auto atEnd() const -> bool { return *m_pos == '\0'; }
    void advance(const unsigned count = 1) { m_pos += count; }
    [[nodiscard]] auto extract() const -> std::string {
        return { m_start, static_cast<std::size_t>(m_pos - m_start) };
    }
    [[nodiscard]] auto makeToken(const TokenKind kind, const KeywordKind kwKind = KeywordKind::None, const OperatorKind opKind = OperatorKind::None) const -> Token {
        return { kind, kwKind, opKind, extract() };
    }

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
    [[nodiscard]] auto classifyWord(std::string_view text) const -> TokenInfo;

    // Keyword lookup table (built once)
    std::unordered_map<std::string, TokenInfo> m_keywords;

    // Operator helpers
    [[nodiscard]] auto operatorToken(OperatorKind kind) -> Token;
    [[nodiscard]] auto operatorToken(OperatorKind kind, unsigned len) -> Token;

    // Scanning state (per tokenise() call)
    const char* m_pos = nullptr;
    const char* m_start = nullptr;
    bool m_atLineStart = true;
    bool m_canBeUnary = true; // true when next +/-/*/@  would be unary
};

} // namespace fbide::lexer
