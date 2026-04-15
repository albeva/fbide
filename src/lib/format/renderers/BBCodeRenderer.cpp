//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "BBCodeRenderer.hpp"
#include "lib/config/Theme.hpp"
using namespace fbide;

namespace {

auto hexColour(const wxColour& colour) -> std::string {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", colour.Red(), colour.Green(), colour.Blue());
    return buf;
}

auto tokenToItemKind(const lexer::TokenKind kind) -> Theme::ItemKind {
    switch (kind) {
        case lexer::TokenKind::Keyword1: return Theme::Keyword;
        case lexer::TokenKind::Keyword2: return Theme::Keyword2;
        case lexer::TokenKind::Keyword3: return Theme::Keyword3;
        case lexer::TokenKind::Keyword4: return Theme::Keyword4;
        case lexer::TokenKind::Comment: return Theme::Comment;
        case lexer::TokenKind::CommentBlock: return Theme::Comment;
        case lexer::TokenKind::String: return Theme::String;
        case lexer::TokenKind::UnterminatedString: return Theme::String;
        case lexer::TokenKind::Number: return Theme::Number;
        case lexer::TokenKind::Preprocessor: return Theme::Preprocessor;
        case lexer::TokenKind::Operator: return Theme::Operator;
        default: return Theme::Default;
    }
}

auto needsStyling(const lexer::TokenKind kind) -> bool {
    return kind != lexer::TokenKind::Identifier
        && kind != lexer::TokenKind::Invalid
        && kind != lexer::TokenKind::Whitespace
        && kind != lexer::TokenKind::Newline;
}

} // namespace

auto BBCodeRenderer::render(const std::vector<lexer::Token>& tokens) const -> wxString {
    std::string output;
    output.reserve(getSizeHint() + tokens.size() * 30);
    output += "[code]\n";

    for (const auto& tok : tokens) {
        if (!needsStyling(tok.kind)) {
            output += tok.text;
            continue;
        }

        const auto& style = m_theme.getStyle(tokenToItemKind(tok.kind));
        const auto colour = hexColour(style.foreground.IsOk() ? style.foreground : m_theme.getDefault().foreground);

        if (style.fontStyle.bold) {
            output += "[b]";
        }
        if (style.fontStyle.italic) {
            output += "[i]";
        }
        output += "[color=" + colour + "]";
        output += tok.text;
        output += "[/color]";
        if (style.fontStyle.italic) {
            output += "[/i]";
        }
        if (style.fontStyle.bold) {
            output += "[/b]";
        }
    }

    output += "\n[/code]";
    return wxString::FromUTF8(output);
}
