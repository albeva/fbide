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
    return tkn.kind == lexer::TokenKind::Keyword1
        || tkn.kind == lexer::TokenKind::Keyword2
        || tkn.kind == lexer::TokenKind::Keyword3
        || tkn.kind == lexer::TokenKind::Keyword4;
}

auto asciiUpper(const char ch) -> char {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}
} // namespace

auto CaseTransform::apply(const std::vector<lexer::Token>& tokens, std::vector<std::string>& pool) -> std::vector<lexer::Token> {
    std::vector result { tokens };

    for (auto& tok : result) {
        if (not isKeyword(tok)) {
            continue;
        }
        std::string converted { tok.text };

        switch (m_mode) {
        case CaseMode::Mixed:
            converted[0] = asciiUpper(converted[0]);
            for (std::size_t i = 1; i < converted.size(); i++) {
                converted[i] = asciiLower(converted[i]);
            }
            break;
        case CaseMode::Upper:
            for (auto& c : converted) {
                c = asciiUpper(c);
            }
            break;
        case CaseMode::Lower:
            for (auto& c : converted) {
                c = asciiLower(c);
            }
            break;
        }

        pool.emplace_back(std::move(converted));
        tok.text = pool.back();
    }

    return result;
}
