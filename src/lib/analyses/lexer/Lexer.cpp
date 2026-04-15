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

} // namespace

Lexer::Lexer(const Keywords& keywords) {
    for (std::size_t i = 0; i < m_keywords.size(); i++) {
        wxStringTokenizer tokenizer(keywords.getGroup(i));
        while (tokenizer.HasMoreTokens()) {
            m_keywords[i].Add(tokenizer.GetNextToken().Upper());
        }
    }
}

auto Lexer::classifyWord(const wxString& word) const -> TokenKind {
    if (word.empty()) {
        return TokenKind::Identifier;
    }

    if (isNumericLiteral(word)) {
        return TokenKind::Number;
    }

    const auto upper = word.Upper();
    constexpr TokenKind groups[] = {
        TokenKind::Keyword1, TokenKind::Keyword2,
        TokenKind::Keyword3, TokenKind::Keyword4
    };
    for (std::size_t i = 0; i < m_keywords.size(); i++) {
        if (m_keywords[i].Index(upper) != wxNOT_FOUND) {
            return groups[i];
        }
    }

    return TokenKind::Identifier;
}

auto Lexer::tokenise(const wxString& source) const -> std::vector<Token> {
    std::vector<Token> tokens;
    const auto len = source.length();
    tokens.reserve(len / 5); // Assime each token, on average, to be 5 characters long
    std::size_t pos = 0;
    bool atLineStart = true;

    while (pos < len) {
        const auto ch = source[pos];

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
            tokens.push_back({ TokenKind::Newline, text });
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
            tokens.push_back({ TokenKind::Whitespace, text });
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
            tokens.push_back({ TokenKind::CommentBlock, text });
            continue;
        }

        // Single-line comment (')
        if (ch == '\'') {
            wxString text;
            while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::Comment, text });
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
            tokens.push_back({ TokenKind::String, text });
            continue;
        }

        // Preprocessor directive (# at start of line)
        if (ch == '#' && atLineStart) {
            wxString text;
            while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                text += source[pos];
                pos++;
            }
            tokens.push_back({ TokenKind::Preprocessor, text });
            continue;
        }

        // Operator
        if (isOperatorChar(ch)) {
            tokens.push_back({ TokenKind::Operator, wxString(ch) });
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

            // Check for REM comment
            if (word.Upper() == "REM" && (pos >= len || !isWordChar(source[pos]))) {
                // Rest of line is comment
                while (pos < len && source[pos] != '\n' && source[pos] != '\r') {
                    word += source[pos];
                    pos++;
                }
                tokens.push_back({ TokenKind::Comment, word });
                continue;
            }

            tokens.push_back({ classifyWord(word), word });
            continue;
        }

        // Anything else — single character as identifier
        tokens.push_back({ TokenKind::Identifier, wxString(ch) });
        pos++;
    }

    return tokens;
}
