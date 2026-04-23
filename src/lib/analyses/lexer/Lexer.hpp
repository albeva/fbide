//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <span>
#include "Token.hpp"
#include "config/ThemeCategory.hpp"

namespace fbide::lexer {

/// Keyword lookup result: which group and structural role.
struct TokenInfo {
    TokenKind tokenKind;
    KeywordKind keywordKind;
};

/// Which lexing context a keyword group applies to.
enum class KeywordScope : std::uint8_t {
    Code,         ///< Normal code (outside asm blocks)
    Asm,          ///< Only inside `asm ... end asm`
    Preprocessor, ///< Only on `#` directive lines (accepted for symmetry)
};

/// One keyword group: the word list, its TokenKind, and the scope it applies in.
struct KeywordGroup {
    wxString keywords;
    TokenKind tokenKind = TokenKind::Identifier;
    KeywordScope scope = KeywordScope::Code;
};

/// Map a ThemeCategory keyword group to the corresponding Lexer TokenKind.
/// Non-keyword categories fall back to Identifier.
constexpr auto tokenKindFor(const ThemeCategory cat) noexcept -> TokenKind {
    switch (cat) {
    case ThemeCategory::Keyword1:       return TokenKind::Keyword1;
    case ThemeCategory::Keyword2:       return TokenKind::Keyword2;
    case ThemeCategory::Keyword3:       return TokenKind::Keyword3;
    case ThemeCategory::Keyword4:       return TokenKind::Keyword4;
    case ThemeCategory::KeywordCustom1: return TokenKind::KeywordCustom1;
    case ThemeCategory::KeywordCustom2: return TokenKind::KeywordCustom2;
    case ThemeCategory::KeywordPP:      return TokenKind::KeywordPP;
    case ThemeCategory::KeywordAsm1:    return TokenKind::KeywordAsm1;
    case ThemeCategory::KeywordAsm2:    return TokenKind::KeywordAsm2;
    default:                            return TokenKind::Identifier;
    }
}

/// Map a ThemeCategory keyword group to the lexing scope it belongs to.
constexpr auto scopeFor(const ThemeCategory cat) noexcept -> KeywordScope {
    switch (cat) {
    case ThemeCategory::KeywordAsm1:
    case ThemeCategory::KeywordAsm2:
        return KeywordScope::Asm;
    case ThemeCategory::KeywordPP:
        return KeywordScope::Preprocessor;
    default:
        return KeywordScope::Code;
    }
}

/// Simple FreeBASIC lexer that tokenises source code.
/// Operates on null-terminated UTF-8 input. Keywords, numbers, and operators
/// are ASCII; UTF-8 multi-byte sequences are valid in identifiers, comments,
/// and string literals.
class Lexer final {
public:
    NO_COPY_AND_MOVE(Lexer)

    /// Construct from a set of keyword groups. Each group specifies its
    /// TokenKind and the scope it applies in (Code / Asm / Preprocessor).
    /// Inside an `asm ... end asm` block only Asm-scoped groups are consulted
    /// for non-structural words; the structural `end`/`asm` pair is detected
    /// through the Code-scoped lookup regardless.
    explicit Lexer(std::span<const KeywordGroup> keywordGroups);

    /// Tokenise the given null-terminated UTF-8 source.
    [[nodiscard]] auto tokenise(const char* source) -> std::vector<Token>;

    /// Tokenise into an existing buffer. Clears `tokens` first then fills
    /// it. Use this on hot paths (per-keystroke lex) to avoid reallocation.
    void tokeniseInto(const char* source, std::vector<Token>& tokens);

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
        return Token { .kind = kind, .keywordKind = kwKind, .operatorKind = opKind, .text = extract() };
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
    [[nodiscard]] auto identifier(bool firstOnLine) -> Token;

    /// Peek past whitespace/tabs on the current line; true when the next
    /// word is `asm` followed by a non-identifier char. Used to detect
    /// `end asm` while already positioned just after the `end` token.
    [[nodiscard]] auto peekEndAsm() const -> bool;

    /// Scan the token stream for `' format off` / `' format on` pragma
    /// comments and mark every token inside a region with `verbatim = true`.
    /// Pragmas must be single-line comments (`'` or `REM` form) alone on
    /// their line; body must match `^\s*format\s+(on|off)\s*$` case-
    /// insensitive. Nested pragmas adjust a depth counter; tokens are
    /// verbatim iff any enclosing depth is > 0. Unbalanced `on` is a no-op.
    /// Unbalanced `off` leaves the rest of the file verbatim.
    static void annotateVerbatim(std::vector<Token>& tokens);

    // Per-scope keyword lookup tables (built once).
    // Preprocessor scope is accepted and stored for API symmetry with the
    // Scintilla lexer; token-level classification on `#` lines currently
    // happens via the internal ppKeywords table in Lexer.cpp.
    std::unordered_map<std::string, TokenInfo> m_codeKeywords;
    std::unordered_map<std::string, TokenInfo> m_asmKeywords;
    std::unordered_map<std::string, TokenInfo> m_ppKeywords;

    // Operator helpers
    [[nodiscard]] auto operatorToken(OperatorKind kind) -> Token;
    [[nodiscard]] auto operatorToken(OperatorKind kind, unsigned len) -> Token;

    // Scanning state (per tokenise() call)
    const char* m_pos = nullptr;
    const char* m_start = nullptr;
    bool m_atLineStart = true;
    bool m_canBeUnary = true; // true when next +/-/*/@  would be unary
    bool m_inAsmBlock = false; // inside `asm ... end asm`
};

} // namespace fbide::lexer
