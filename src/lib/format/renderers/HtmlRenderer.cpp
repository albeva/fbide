//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "HtmlRenderer.hpp"
#include "config/Theme.hpp"
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

auto escapeHtml(std::string_view text) -> std::string {
    std::string escaped;
    escaped.reserve(text.size());
    for (const auto ch : text) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

} // namespace

auto HtmlRenderer::render(const std::vector<lexer::Token>& tokens) const -> wxString {
    const auto& editor = m_theme.getDefault();
    std::string output;
    output.reserve(getSizeHint() + tokens.size() * 50); // html needs lot of markup
    output += "<pre>";

    for (const auto& tok : tokens) {
        const auto escaped = escapeHtml(tok.text);

        if (!needsStyling(tok.kind)) {
            output += escaped;
            continue;
        }

        const auto& style = m_theme.getStyle(tokenToItemKind(tok.kind));
        const auto colour = hexColour(style.foreground.IsOk() ? style.foreground : editor.foreground);

        std::string css = "color:" + colour;
        if (style.fontStyle.bold) {
            css += ";font-weight:bold";
        }
        if (style.fontStyle.italic) {
            css += ";font-style:italic";
        }
        if (style.fontStyle.underline) {
            css += ";text-decoration:underline";
        }

        output += "<span style=\"" + css + "\">" + escaped + "</span>";
    }

    output += "</pre>";
    return wxString::FromUTF8(output);
}

auto HtmlRenderer::decorate(const wxString& rendered)-> wxString {
    wxString output;
    output += "<!DOCTYPE html>\n<html>\n<head>\n";
    output += "<meta charset=\"utf-8\">\n";
    output += "<title>FBIde Export</title>\n";
    output += "</head>\n";
    output += "<body>" + rendered + "</body>";
    output += "</html>\n";
    return output;
}
