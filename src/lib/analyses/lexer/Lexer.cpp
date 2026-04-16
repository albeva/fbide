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

/// ASCII word character: alphanumeric, underscore, dollar sign.
/// FB identifiers are ASCII only — non-ASCII bytes are invalid outside
/// comments and string literals.
auto isWordChar(const char ch) -> bool {
    if (ch >= 'a' && ch <= 'z') return true;
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= '0' && ch <= '9') return true;
    return ch == '_' || ch == '$';
}

auto isDigit(const char ch) -> bool {
    return ch >= '0' && ch <= '9';
}

auto isAlnum(const char ch) -> bool {
    if (ch >= 'a' && ch <= 'z') return true;
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= '0' && ch <= '9') return true;
    return false;
}

auto asciiUpper(const char ch) -> char {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

/// Map of structurally significant keywords to their KeywordKind.
const std::unordered_map<std::string, KeywordKind> structuralKeywords = {
    // Block openers
    { "sub",         KeywordKind::Sub },
    { "function",    KeywordKind::Function },
    { "constructor", KeywordKind::Constructor },
    { "destructor",  KeywordKind::Destructor },
    { "operator",    KeywordKind::Operator },
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
    // Declaration
    { "declare",  KeywordKind::Declare },
    // Comment keyword
    { "rem",      KeywordKind::Rem },
};

/// Preprocessor directive keywords mapped to their KeywordKind.
const std::unordered_map<std::string, KeywordKind> ppKeywords = {
    // Block openers
    { "if",          KeywordKind::PpIf },
    { "ifdef",       KeywordKind::PpIfDef },
    { "ifndef",      KeywordKind::PpIfNDef },
    { "macro",       KeywordKind::PpMacro },
    // Block closers
    { "endif",       KeywordKind::PpEndIf },
    { "endmacro",    KeywordKind::PpEndMacro },
    // Mid-block
    { "else",        KeywordKind::PpElse },
    { "elseif",      KeywordKind::PpElseIf },
    { "elseifdef",   KeywordKind::PpElseIfDef },
    { "elseifndef",  KeywordKind::PpElseIfNDef },
};

/// Lowercase an ASCII string_view into a std::string.
auto toLower(const std::string_view sv) -> std::string {
    std::string result(sv.size(), '\0');
    for (std::size_t i = 0; i < sv.size(); i++) {
        result[i] = asciiLower(sv[i]);
    }
    return result;
}

/// Case-insensitive ASCII compare (3 chars max for "rem").
auto isRem(const std::string_view sv) -> bool {
    return sv.size() == 3
        && asciiLower(sv[0]) == 'r'
        && asciiLower(sv[1]) == 'e'
        && asciiLower(sv[2]) == 'm';
}

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
            auto key = tokenizer.GetNextToken().ToStdString(wxConvUTF8);
            auto lower = toLower(key);
            auto it = structuralKeywords.find(lower);
            const auto kwKind = (it != structuralKeywords.end()) ? it->second : KeywordKind::Other;
            m_keywords.emplace(std::move(lower), TokenInfo { groups[i], kwKind });
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto Lexer::tokenise(const char* source) -> std::vector<Token> {
    m_pos = source;
    m_start = source;
    m_atLineStart = true;
    m_canBeUnary = true;

    std::vector<Token> tokens;
    // Estimate: one token per ~5 chars
    tokens.reserve(std::strlen(source) / 5);

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

    switch (ch) {
    // Newlines
    case '\n':
    case '\r':
        return newline();

    // Whitespace
    case ' ':
    case '\t':
        return whitespace();

    // Single-line comment
    case '\'':
        m_atLineStart = false;
        return comment();

    // Block comment opener or '/' operator
    case '/':
        m_atLineStart = false;
        if (peek() == '\'') {
            m_canBeUnary = true;
            return commentBlock();
        }
        if (peek() == '=') {
            return operatorToken(OperatorKind::DivAssign, 2);
        }
        return operatorToken(OperatorKind::Divide);

    // String literal or '!' operator
    case '!':
        if (peek() == '"') {
            m_atLineStart = false;
            m_canBeUnary = false;
            advance(); // skip '!'
            return stringLiteral(StringMode::Escaped);
        }
        return operatorToken(OperatorKind::Exclamation);

    case '$':
        if (peek() == '"') {
            m_atLineStart = false;
            m_canBeUnary = false;
            advance(); // skip '$'
            return stringLiteral(StringMode::Normal);
        }
        // '$' is a word char — handle as identifier
        m_atLineStart = false;
        m_canBeUnary = false;
        return identifier();

    case '"':
        m_atLineStart = false;
        m_canBeUnary = false;
        return stringLiteral(StringMode::Normal);

    // Preprocessor directive
    case '#':
        if (m_atLineStart) {
            m_atLineStart = false;
            m_canBeUnary = true;
            return preprocessor();
        }
        return operatorToken(OperatorKind::Hash);

    // '.' — operator, or number if followed by digit (.3)
    case '.': {
        m_atLineStart = false;
        if (isDigit(peek())) {
            m_canBeUnary = false;
            return number();
        }
        advance();
        if (current() == '.') {
            advance();
            if (current() == '.') {
                advance();
                m_canBeUnary = true;
                return makeToken(TokenKind::Operator, KeywordKind::None, OperatorKind::Ellipsis3);
            }
            m_canBeUnary = true;
            return makeToken(TokenKind::Operator, KeywordKind::None, OperatorKind::Ellipsis2);
        }
        m_canBeUnary = false;
        return makeToken(TokenKind::Operator, KeywordKind::None, OperatorKind::Dot);
    }

    // Compound operators starting with '<'
    case '<':
        m_atLineStart = false;
        if (peek() == '<') {
            if (m_pos[2] == '=') {
                return operatorToken(OperatorKind::ShlAssign, 3);
            }
            return operatorToken(OperatorKind::ShiftLeft, 2);
        }
        if (peek() == '>') return operatorToken(OperatorKind::NotEqual, 2);
        if (peek() == '=') return operatorToken(OperatorKind::LessEqual, 2);
        return operatorToken(OperatorKind::Less);

    // Compound operators starting with '>'
    case '>':
        m_atLineStart = false;
        if (peek() == '>') {
            if (m_pos[2] == '=') {
                return operatorToken(OperatorKind::ShrAssign, 3);
            }
            return operatorToken(OperatorKind::ShiftRight, 2);
        }
        if (peek() == '=') return operatorToken(OperatorKind::GreaterEqual, 2);
        return operatorToken(OperatorKind::Greater);

    // '-' — subtract, negate, -=, ->
    case '-':
        m_atLineStart = false;
        if (peek() == '=') return operatorToken(OperatorKind::SubAssign, 2);
        if (peek() == '>') return operatorToken(OperatorKind::Arrow, 2);
        return operatorToken(m_canBeUnary ? OperatorKind::Negate : OperatorKind::Subtract);

    // '+' — add, unary plus, +=
    case '+':
        m_atLineStart = false;
        if (peek() == '=') return operatorToken(OperatorKind::AddAssign, 2);
        return operatorToken(m_canBeUnary ? OperatorKind::UnaryPlus : OperatorKind::Add);

    // '*' — multiply, dereference, *=
    case '*':
        m_atLineStart = false;
        if (peek() == '=') return operatorToken(OperatorKind::MulAssign, 2);
        return operatorToken(m_canBeUnary ? OperatorKind::Dereference : OperatorKind::Multiply);

    // '=' — always Assign (formatter treats = and == the same for spacing)
    case '=':
        return operatorToken(OperatorKind::Assign);

    // '\' — integer divide, \=
    case '\\':
        m_atLineStart = false;
        if (peek() == '=') return operatorToken(OperatorKind::IntDivAssign, 2);
        return operatorToken(OperatorKind::IntDivide);

    // '^' — exponentiate, ^=
    case '^':
        m_atLineStart = false;
        if (peek() == '=') return operatorToken(OperatorKind::ExpAssign, 2);
        return operatorToken(OperatorKind::Exponentiate);

    // '@' — always unary (address of)
    case '@':
        return operatorToken(OperatorKind::AddressOf);

    // Simple single-char operators
    case '(':  return operatorToken(OperatorKind::ParenOpen);
    case ')':  return operatorToken(OperatorKind::ParenClose);
    case '[':  return operatorToken(OperatorKind::BracketOpen);
    case ']':  return operatorToken(OperatorKind::BracketClose);
    case '{':  return operatorToken(OperatorKind::BraceOpen);
    case '}':  return operatorToken(OperatorKind::BraceClose);
    case ',':  return operatorToken(OperatorKind::Comma);
    case ';':  return operatorToken(OperatorKind::Semicolon);
    case ':':  return operatorToken(OperatorKind::Colon);
    case '?':  return operatorToken(OperatorKind::Question);
    case '%':  return operatorToken(OperatorKind::Percent);
    case '~':  // bitwise not symbol — treat as generic operator for now
    case '|':  // pipe — not a standard FB operator but accept it
        m_atLineStart = false;
        m_canBeUnary = true;
        advance();
        return makeToken(TokenKind::Operator);

    // '&' — concatenate, &=, or number prefix &H/&O/&B
    case '&': {
        m_atLineStart = false;
        const auto next = asciiUpper(peek());
        if (next == 'H' || next == 'O' || next == 'B') {
            m_canBeUnary = false;
            advance(); // skip '&'
            return number();
        }
        if (peek() == '=') return operatorToken(OperatorKind::ConcatAssign, 2);
        return operatorToken(OperatorKind::Concatenate);
    }
    // clang-format off
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    // clang-format on
        m_atLineStart = false;
        return number();

    default:
        m_atLineStart = false;
        if (isWordChar(ch)) {
            return identifier();
        }
        // Unrecognised input
        advance();
        return makeToken(TokenKind::Invalid);
    }
}

// ---------------------------------------------------------------------------
// Token producers
// ---------------------------------------------------------------------------

auto Lexer::operatorToken(const OperatorKind kind) -> Token {
    m_atLineStart = false;
    advance();
    // Update unary context: after closing brackets → binary, everything else → unary
    switch (kind) {
    case OperatorKind::ParenClose:
    case OperatorKind::BracketClose:
    case OperatorKind::BraceClose:
        m_canBeUnary = false;
        break;
    default:
        m_canBeUnary = true;
        break;
    }
    return makeToken(TokenKind::Operator, KeywordKind::None, kind);
}

auto Lexer::operatorToken(const OperatorKind kind, const unsigned len) -> Token {
    m_atLineStart = false;
    advance(len);
    m_canBeUnary = true;
    return makeToken(TokenKind::Operator, KeywordKind::None, kind);
}

auto Lexer::newline() -> Token {
    advance();
    // Handle \r\n
    if (*m_start == '\r' && !atEnd() && current() == '\n') {
        advance();
    }
    m_atLineStart = true;
    m_canBeUnary = true;
    return makeToken(TokenKind::Newline);
}

auto Lexer::whitespace() -> Token {
    while (current() == ' ' || current() == '\t') {
        advance();
    }
    return makeToken(TokenKind::Whitespace);
}

auto Lexer::comment() -> Token {
    while (!atEnd() && current() != '\n' && current() != '\r') {
        advance();
    }
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
    // Skip '#' (already at m_start)
    advance();
    // Skip optional whitespace after '#'
    while (current() == ' ' || current() == '\t') {
        advance();
    }
    // Read directive word
    const auto* dirStart = m_pos;
    while (!atEnd() && ((current() >= 'a' && current() <= 'z') || (current() >= 'A' && current() <= 'Z'))) {
        advance();
    }
    // Classify directive
    const auto directive = toLower({ dirStart, static_cast<std::size_t>(m_pos - dirStart) });
    auto kwKind = KeywordKind::PpOther;
    if (const auto it = ppKeywords.find(directive); it != ppKeywords.end()) {
        kwKind = it->second;
    }
    // Consume rest of line
    while (!atEnd() && current() != '\n' && current() != '\r') {
        advance();
    }
    return makeToken(TokenKind::Preprocessor, kwKind);
}

auto Lexer::number() -> Token {
    // Consume digits, letters (hex, exponent, type suffix), '.', and '_'
    // Also consume '+'/'-' after exponent marker (e/E/d/D) in floating-point mode
    bool fp = current() == '.';
    char prev = '\0';
    while (!atEnd()) {
        const auto ch = current();
        if (ch == '.') {
            fp = true;
            prev = ch;
            advance();
        } else if (isAlnum(ch) || ch == '_') {
            prev = ch;
            advance();
        } else if (fp && (ch == '+' || ch == '-') && (prev == 'e' || prev == 'E' || prev == 'd' || prev == 'D')) {
            prev = ch;
            advance();
        } else {
            break;
        }
    }
    m_canBeUnary = false;
    return makeToken(TokenKind::Number);
}

auto Lexer::identifier() -> Token {
    while (!atEnd() && isWordChar(current())) {
        advance();
    }
    const auto text = extract();

    // REM is a comment keyword — rest of line is comment
    if (isRem(text)) {
        while (!atEnd() && current() != '\n' && current() != '\r') {
            advance();
        }
        m_canBeUnary = true;
        return makeToken(TokenKind::Comment);
    }

    // Classify as keyword or identifier
    const auto info = classifyWord(text);
    // After identifiers → binary. After keyword operators (And, Not, etc.) → unary.
    m_canBeUnary = (info.tokenKind != TokenKind::Identifier);
    return { info.tokenKind, info.keywordKind, OperatorKind::None, text };
}

// ---------------------------------------------------------------------------
// Keyword classification
// ---------------------------------------------------------------------------

auto Lexer::classifyWord(const std::string_view text) const -> TokenInfo {
    const auto key = toLower(text);
    const auto it = m_keywords.find(key);
    if (it != m_keywords.end()) {
        return it->second;
    }
    return { .tokenKind = TokenKind::Identifier, .keywordKind = KeywordKind::None };
}
