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

auto hexColour(const wxColour& colour) -> wxString {
    return wxString::Format("#%02X%02X%02X", colour.Red(), colour.Green(), colour.Blue());
}

auto tokenToItemKind(const TokenKind kind) -> Theme::ItemKind {
    switch (kind) {
        case TokenKind::Keyword1: return Theme::Keyword;
        case TokenKind::Keyword2: return Theme::Keyword2;
        case TokenKind::Keyword3: return Theme::Keyword3;
        case TokenKind::Keyword4: return Theme::Keyword4;
        case TokenKind::Comment: return Theme::Comment;
        case TokenKind::String: return Theme::String;
        case TokenKind::Number: return Theme::Number;
        case TokenKind::Preprocessor: return Theme::Preprocessor;
        case TokenKind::Operator: return Theme::Operator;
        default: return Theme::Default;
    }
}

auto needsStyling(const TokenKind kind) -> bool {
    return kind != TokenKind::Identifier
        && kind != TokenKind::Whitespace
        && kind != TokenKind::Newline;
}

} // namespace

auto BBCodeRenderer::render(const std::vector<Token>& tokens) const -> wxString {
    wxString output;
    output += "[code]\n";

    for (const auto& [kind, text] : tokens) {
        if (!needsStyling(kind)) {
            output += text;
            continue;
        }

        const auto& style = m_theme.getStyle(tokenToItemKind(kind));
        const auto colour = hexColour(style.foreground.IsOk() ? style.foreground : m_theme.getDefault().foreground);

        if (style.fontStyle.bold) {
            output += "[b]";
        }
        if (style.fontStyle.italic) {
            output += "[i]";
        }
        output += "[color=" + colour + "]";
        output += text;
        output += "[/color]";
        if (style.fontStyle.italic) {
            output += "[/i]";
        }
        if (style.fontStyle.bold) {
            output += "[/b]";
        }
    }

    output += "\n[/code]";
    return output;
}
