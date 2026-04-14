//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CaseTransform.hpp"
using namespace fbide;

namespace {

auto isKeyword(const TokenKind kind) -> bool {
    return kind == TokenKind::Keyword1
        || kind == TokenKind::Keyword2
        || kind == TokenKind::Keyword3
        || kind == TokenKind::Keyword4;
}

} // namespace

auto CaseTransform::apply(std::vector<Token> tokens) const -> std::vector<Token> {
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
