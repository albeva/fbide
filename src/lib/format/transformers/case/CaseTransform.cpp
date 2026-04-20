//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CaseTransform.hpp"
using namespace fbide;

namespace {

auto isKeyword(const lexer::Token& tkn) -> bool {
    return lexer::isKeywordToken(tkn.kind);
}

auto asciiUpper(const char ch) -> char {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}
} // namespace

auto CaseTransform::apply(const std::vector<lexer::Token>& tokens) -> std::vector<lexer::Token> {
    std::vector result { tokens };

    for (auto& tok : result) {
        if (not isKeyword(tok)) {
            continue;
        }

        switch (m_mode) {
        case CaseMode::Mixed:
            tok.text[0] = asciiUpper(tok.text[0]);
            for (std::size_t i = 1; i < tok.text.size(); i++) {
                tok.text[i] = asciiLower(tok.text[i]);
            }
            break;
        case CaseMode::Upper:
            for (auto& c : tok.text) {
                c = asciiUpper(c);
            }
            break;
        case CaseMode::Lower:
            for (auto& c : tok.text) {
                c = asciiLower(c);
            }
            break;
        }
    }

    return result;
}
