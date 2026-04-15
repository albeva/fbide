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

} // namespace

auto CaseTransform::apply(std::vector<lexer::Token> tokens) const -> std::vector<lexer::Token> {
    for (auto& [kind, text] : tokens) {
        if (!isKeyword(kind)) {
            continue;
        }
        switch (m_mode) {
            case CaseMode::Mixed:
                text = text.Left(1).Upper() + text.Mid(1).Lower();
                break;
            case CaseMode::Upper:
                text = text.Upper();
                break;
            case CaseMode::Lower:
                text = text.Lower();
                break;
        }
    }
    return tokens;
}
