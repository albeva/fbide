//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CaseTransform.hpp"
using namespace fbide;

namespace {

auto isKeyword(const lexer::TokenKind kind) -> bool {
    return kind == lexer::TokenKind::Keyword1
        || kind == lexer::TokenKind::Keyword2
        || kind == lexer::TokenKind::Keyword3
        || kind == lexer::TokenKind::Keyword4;
}

auto asciiUpper(const char ch) -> char {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

auto asciiLower(const char ch) -> char {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

} // namespace

auto CaseTransform::apply(const std::vector<lexer::Token>& tokens) const -> std::vector<lexer::Token> {
    m_pool.clear();
    m_pool.reserve(tokens.size()); // upper bound: at most one entry per token
    std::vector<lexer::Token> result;
    result.reserve(tokens.size());

    for (const auto& tok : tokens) {
        if (!isKeyword(tok.kind)) {
            result.push_back(tok);
            continue;
        }

        // Keywords are ASCII — simple byte-level case conversion
        std::string converted(tok.text);
        switch (m_mode) {
            case CaseMode::Mixed:
                if (!converted.empty()) {
                    converted[0] = asciiUpper(converted[0]);
                    for (std::size_t i = 1; i < converted.size(); i++) {
                        converted[i] = asciiLower(converted[i]);
                    }
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

        m_pool.push_back(std::move(converted));
        result.push_back({ tok.kind, tok.keywordKind, m_pool.back() });
    }

    return result;
}
