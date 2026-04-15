//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Lexer.hpp"
#include "lib/config/Keywords.hpp"
using namespace fbide::lexer;

namespace {

auto isWordChar(const wxUniChar ch) -> bool {
    return wxIsalnum(ch) || ch == '_' || ch == '$';
}

/// Map of structurally significant keywords to their KeywordKind.
const std::unordered_map<wxString, KeywordKind> structuralKeywords = {
    // Block openers
    { "sub",      KeywordKind::Sub },
    { "function", KeywordKind::Function },
    { "do",       KeywordKind::Do },
    { "while",    KeywordKind::While },
    { "for",      KeywordKind::For },
    { "with",     KeywordKind::With },
    { "scope",    KeywordKind::Scope },
    { "enum",     KeywordKind::Enum },
    { "union",    KeywordKind::Union },
    { "select",   KeywordKind::Select },
    { "asm",      KeywordKind::Asm },
    // Block closers
    { "end",      KeywordKind::End },
    { "loop",     KeywordKind::Loop },
    { "next",     KeywordKind::Next },
    { "wend",     KeywordKind::Wend },
    // Mid-block
    { "else",     KeywordKind::Else },
    { "elseif",   KeywordKind::ElseIf },
    { "case",     KeywordKind::Case },
    // Conditional
    { "if",       KeywordKind::If },
    { "then",     KeywordKind::Then },
    // Type
    { "type",     KeywordKind::Type },
    { "as",       KeywordKind::As },
    // Comment keyword
    { "rem",      KeywordKind::Rem },
};

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Lexer::Lexer(const Keywords& keywords) {
    constexpr TokenKind groups[] = {
        TokenKind::Keyword1, TokenKind::Keyword2,
        TokenKind::Keyword3, TokenKind::Keyword4
    };
    for (std::size_t i = 0; i < 4; i++) {
        wxStringTokenizer tokenizer(keywords.getGroup(i));
        while (tokenizer.HasMoreTokens()) {
            auto key = tokenizer.GetNextToken();
            auto it = structuralKeywords.find(key);
            const auto kwKind = (it != structuralKeywords.end()) ? it->second : KeywordKind::Other;
            m_keywords.emplace(std::move(key), TokenInfo { groups[i], kwKind });
        }
    }
}

// ---------------------------------------------------------------------------
// Cursor helpers
// ---------------------------------------------------------------------------

auto Lexer::current() const -> wxUniChar {
    return (*m_source)[m_pos];
}

auto Lexer::peek() const -> wxUniChar {
    return (m_pos + 1 < m_len) ? (*m_source)[m_pos + 1] : wxUniChar('\0');
}

auto Lexer::atEnd() const -> bool {
    return m_pos >= m_len;
}

void Lexer::advance(const unsigned count) {
    m_pos += count;
}

void Lexer::skipWhile(bool (*pred)(wxUniChar)) {
    while (m_pos < m_len && pred((*m_source)[m_pos])) {
        m_pos++;
    }
}

void Lexer::skipToLineEnd() {
    while (m_pos < m_len && current() != '\n' && current() != '\r') {
        m_pos++;
    }
}

auto Lexer::extract() const -> wxString {
    return m_source->Mid(m_start, m_pos - m_start);
}

auto Lexer::makeToken(const TokenKind kind, const KeywordKind kwKind) const -> Token {
    return { kind, kwKind, extract(), m_start, m_pos };
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto Lexer::tokenise(const wxString& source) -> std::vector<Token> {
    m_source = &source;
    m_len = static_cast<unsigned>(source.length());
    m_pos = 0;
    m_atLineStart = true;

    std::vector<Token> tokens;
    tokens.reserve(m_len / 5);

    while (!atEnd()) {
        tokens.push_back(next());
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// Main dispatch
// ---------------------------------------------------------------------------

auto Lexer::next() -> Token {
    m_start = m_pos;
    const auto ch = current();

    switch (ch.GetValue()) {
    // Newlines
    case '\n':
    case '\r':
        return newline();

    // Whitespace
    case ' ':
    case '\t':
        return whitespace();

    // Single-line comment or block comment closer fragment
    case '\'':
        m_atLineStart = false;
        return comment();

    // Block comment opener or '/' operator
    case '/':
        m_atLineStart = false;
        if (peek() == '\'') {
            return commentBlock();
        }
        advance();
        return makeToken(TokenKind::Operator);

    // String literal
    case '!':
        if (peek() == '"') {
            m_atLineStart = false;
            advance(); // skip '!'
            return stringLiteral(StringMode::Escaped);
        }
        // '!' alone — treat as operator
        m_atLineStart = false;
        advance();
        return makeToken(TokenKind::Operator);

    case '$':
        if (peek() == '"') {
            m_atLineStart = false;
            advance(); // skip '$'
            return stringLiteral(StringMode::Normal);
        }
        // '$' is a word char — handle as word
        m_atLineStart = false;
        return identifier();

    case '"':
        m_atLineStart = false;
        return stringLiteral(StringMode::Normal);

    // Preprocessor directive
    case '#':
        if (m_atLineStart) {
            m_atLineStart = false;
            return preprocessor();
        }
        // '#' not at line start — treat as operator
        m_atLineStart = false;
        advance();
        return makeToken(TokenKind::Operator);

    // '.' — operator, or number if followed by digit (.3)
    case '.': { // ...
        m_atLineStart = false;
        if (wxIsdigit(peek())) {
            return number();
        }
        advance();
        if (!atEnd() && current() == '.') {
            advance();
            if (!atEnd() && current() == '.') {
                advance();
            }
        }
        return makeToken(TokenKind::Operator);
    }
    // clang-format off
    case '(': case ')': case ',':
    case ':': case ';': case '<': case '=': case '>':
    case '?': case '\\': case '^': case '{': case '|':
    case '}': case '~': case '+': case '-': case '*':
    // clang-format on
        m_atLineStart = false;
        advance();
        return makeToken(TokenKind::Operator);
    // &H, &O, &B number prefixes
    case '&': {
        m_atLineStart = false;
        const auto next = wxToupper(peek());
        if (next == 'H' || next == 'O' || next == 'B') {
            advance(); // skip '&'
            return number();
        }
        advance();
        return makeToken(TokenKind::Operator);
    }
    // clang-format off
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    // clang-format on
        m_atLineStart = false;
        return number();

    default:
        m_atLineStart = false;
        // Word (identifier, keyword, or number)
        if (isWordChar(ch)) {
            return identifier();
        }
        // Anything else — unrecognised input
        advance();
        return makeToken(TokenKind::Invalid);
    }
}

// ---------------------------------------------------------------------------
// Token producers
// ---------------------------------------------------------------------------

auto Lexer::newline() -> Token {
    advance();
    // Handle \r\n
    if ((*m_source)[m_start] == '\r' && !atEnd() && current() == '\n') {
        advance();
    }
    m_atLineStart = true;
    return makeToken(TokenKind::Newline);
}

auto Lexer::whitespace() -> Token {
    skipWhile([](const wxUniChar ch) { return ch == ' ' || ch == '\t'; });
    return makeToken(TokenKind::Whitespace);
}

auto Lexer::comment() -> Token {
    skipToLineEnd();
    return makeToken(TokenKind::Comment);
}

auto Lexer::commentBlock() -> Token {
    advance(2); // skip /'
    int depth = 1;
    while (!atEnd() && depth > 0) {
        const auto ch = current();
        if (ch == '/' && peek() == '\'') {
            advance(2);
            depth++;
        } else if (ch == '\'' && peek() == '/') {
            advance(2);
            depth--;
        } else {
            advance();
        }
    }
    return makeToken(TokenKind::CommentBlock);
}

auto Lexer::stringLiteral(const StringMode mode) -> Token {
    advance(); // skip opening quote

    while (!atEnd() && current() != '\n' && current() != '\r') {
        if (mode == StringMode::Escaped && current() == '\\') {
            advance(); // skip backslash
            if (!atEnd()) {
                advance(); // skip escaped char
            }
            continue;
        }
        if (current() == '"') {
            if (peek() == '"') {
                advance(2); // skip doubled quote
                continue;
            }
            advance(); // skip closing quote
            return makeToken(TokenKind::String);
        }
        advance();
    }

    // Unterminated string — don't consume the newline
    return makeToken(TokenKind::UnterminatedString);
}

auto Lexer::preprocessor() -> Token {
    skipToLineEnd();
    return makeToken(TokenKind::Preprocessor);
}

auto Lexer::number() -> Token {
    // Consume digits, letters (hex, exponent, type suffix), '.', and '_'
    // Also consume '+'/'-' after exponent marker (e/E/d/D) in floating-point mode
    bool fp = current() == '.';
    wxUniChar prev = '\0';
    while (!atEnd()) {
        const auto ch = current();
        if (ch == '.') {
            fp = true;
            prev = ch;
            advance();
        } else if (wxIsalnum(ch) || ch == '_') {
            prev = ch;
            advance();
        } else if (fp && (ch == '+' || ch == '-') && (prev == 'e' || prev == 'E' || prev == 'd' || prev == 'D')) {
            prev = ch;
            advance();
        } else {
            break;
        }
    }
    return makeToken(TokenKind::Number);
}

auto Lexer::identifier() -> Token {
    skipWhile(isWordChar);
    const auto text = extract();

    // REM is a comment keyword — rest of line is comment
    if (text.IsSameAs("rem", false)) {
        skipToLineEnd();
        return makeToken(TokenKind::Comment);
    }

    // Classify as keyword or identifier
    auto info = classifyWord(text);
    return { info.tokenKind, info.keywordKind, text, m_start, m_pos };
}

// ---------------------------------------------------------------------------
// Keyword classification
// ---------------------------------------------------------------------------

auto Lexer::classifyWord(const wxString& text) const -> TokenInfo {
    const auto it = m_keywords.find(text.Lower());
    if (it != m_keywords.end()) {
        return it->second;
    }
    return { .tokenKind = TokenKind::Identifier, .keywordKind = KeywordKind::None };
}
