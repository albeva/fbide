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

auto isOperatorChar(const wxUniChar ch) -> bool {
    switch (ch.GetValue()) {
        case '(': case ')': case ',': case '.': case '/':
        case ':': case ';': case '<': case '=': case '>':
        case '?': case '\\': case '^': case '{': case '|':
        case '}': case '~': case '+': case '-': case '*':
            return true;
        default:
            return false;
    }
}

auto isWordChar(const wxUniChar ch) -> bool {
    if (wxIsalnum(ch) || ch == '_' || ch == '$') {
        return true;
    }
    return false;
}

auto isNumericLiteral(const wxString& word) -> bool {
    if (word.empty()) {
        return false;
    }

    // &H (hex), &O (octal), &B (binary) prefixes
    if (word.length() >= 2 && word[0] == '&') {
        const auto prefix = wxToupper(word[1]);
        if (prefix == 'H' || prefix == 'O' || prefix == 'B') {
            return true;
        }
    }

    return wxIsdigit(word[0]);
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
            m_keywords.emplace(std::move(key), KeywordInfo { groups[i], kwKind });
        }
    }
}

auto Lexer::classifyWord(const wxString& word) const -> std::pair<TokenKind, KeywordKind> {
    if (word.empty()) {
        return { TokenKind::Identifier, KeywordKind::None };
    }

    if (isNumericLiteral(word)) {
        return { TokenKind::Number, KeywordKind::None };
    }

    const auto it = m_keywords.find(word.Lower());
    if (it != m_keywords.end()) {
        return { it->second.tokenKind, it->second.keywordKind };
    }

    return { TokenKind::Identifier, KeywordKind::None };
}

auto Lexer::tokenise(const wxString& source) const -> std::vector<Token> {
    std::vector<Token> tokens;
    const auto len = source.length();
    tokens.reserve(len / 5); // Assume each token, on average, to be 5 characters long
    std::size_t pos = 0;
    bool atLineStart = true;

    while (pos < len) {
        const auto ch = source[pos];
        const auto start = pos;

        // Newline
        if (ch == '\n' || ch == '\r') {
            wxString text;
            text += ch;
            pos++;
            // Handle \r\n
            if (ch == '\r' && pos < len && source[pos] == '\n') {
                text += '\n';
                pos++;
            }
            tokens.push_back({ TokenKind::Newline, KeywordKind::None, text, start, pos });
            atLineStart = true;
            continue;
        }

        // Whitespace
        if (ch == ' ' || ch == '\t') {
            wxString text;
            while (pos < len && (source[pos] == ' ' || source[pos] == '\t')) {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::Whitespace, KeywordKind::None, text, start, pos });
            continue;
        }

        atLineStart = false;

        // Multi-line comment (/' ... '/)  — may nest
        if (ch == '/' && pos + 1 < len && source[pos + 1] == '\'') {
            wxString text;
            text += source[pos];     // /
            text += source[pos + 1]; // '
            pos += 2;
            int depth = 1;
            while (pos < len && depth > 0) {
                if (source[pos] == '/' && pos + 1 < len && source[pos + 1] == '\'') {
                    text += source[pos];
                    text += source[pos + 1];
                    pos += 2;
                    depth++;
                } else if (source[pos] == '\'' && pos + 1 < len && source[pos + 1] == '/') {
                    text += source[pos];
                    text += source[pos + 1];
                    pos += 2;
                    depth--;
                } else {
                    text += source[pos];
                    pos++;
                }
            }
            tokens.push_back({ TokenKind::CommentBlock, KeywordKind::None, text, start, pos });
            continue;
        }

        // Single-line comment (')
        if (ch == '\'') {
            wxString text;
            while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::Comment, KeywordKind::None, text, start, pos });
            continue;
        }

        // Double-quoted string
        if (ch == '"') {
            wxString text;
            text += ch;
            pos++;
            while (pos < len && source[pos] != '"' && source[pos] != '\n' && source[pos] != '\r') {
                text += source[pos];
                pos++;
            }
            if (pos < len && source[pos] == '"') {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::String, KeywordKind::None, text, start, pos });
            continue;
        }

        // Preprocessor directive (# at start of line)
        if (ch == '#' && atLineStart) {
            wxString text;
            while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::Preprocessor, KeywordKind::None, text, start, pos });
            continue;
        }

        // Operator
        if (isOperatorChar(ch)) {
            tokens.push_back({ TokenKind::Operator, KeywordKind::None, wxString(ch), start, start + 1 });
            pos++;
            continue;
        }

        // Word (identifier, keyword, or number)
        if (isWordChar(ch)) {
            wxString word;
            while (pos < len && isWordChar(source[pos])) {
                word += source[pos];
                pos++;
            }

            auto [kind, kwKind] = classifyWord(word);

            // Check for REM comment
            if (kwKind == KeywordKind::Rem && (pos >= len || !isWordChar(source[pos]))) {
                // Rest of line is comment
                while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                    word += source[pos];
                    pos++;
                }
                tokens.push_back({ TokenKind::Comment, KeywordKind::None, word, start, pos });
                continue;
            }

            tokens.push_back({ kind, kwKind, word, start, pos });
            continue;
        }

        // Anything else — single character as identifier
        tokens.push_back({ TokenKind::Identifier, KeywordKind::None, wxString(ch), start, start + 1 });
        pos++;
    }

    return tokens;
}
