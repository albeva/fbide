//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "HtmlRenderer.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
using namespace fbide;

namespace {

auto hexColour(const wxColour& colour) -> std::string {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", colour.Red(), colour.Green(), colour.Blue());
    return buf;
}

auto tokenToCategory(const lexer::TokenKind kind) -> ThemeCategory {
    switch (kind) {
    case lexer::TokenKind::Keyword1:
        return ThemeCategory::Keyword1;
    case lexer::TokenKind::Keyword2:
        return ThemeCategory::Keyword2;
    case lexer::TokenKind::Keyword3:
        return ThemeCategory::Keyword3;
    case lexer::TokenKind::Keyword4:
        return ThemeCategory::Keyword4;
    case lexer::TokenKind::KeywordCustom1:
        return ThemeCategory::KeywordCustom1;
    case lexer::TokenKind::KeywordCustom2:
        return ThemeCategory::KeywordCustom2;
    case lexer::TokenKind::KeywordPP:
        return ThemeCategory::KeywordPP;
    case lexer::TokenKind::KeywordAsm1:
        return ThemeCategory::KeywordAsm1;
    case lexer::TokenKind::KeywordAsm2:
        return ThemeCategory::KeywordAsm2;
    case lexer::TokenKind::Comment:
        return ThemeCategory::Comment;
    case lexer::TokenKind::CommentBlock:
        return ThemeCategory::MultilineComment;
    case lexer::TokenKind::String:
        return ThemeCategory::String;
    case lexer::TokenKind::UnterminatedString:
        return ThemeCategory::StringOpen;
    case lexer::TokenKind::Number:
        return ThemeCategory::Number;
    case lexer::TokenKind::Preprocessor:
        return ThemeCategory::Preprocessor;
    case lexer::TokenKind::Operator:
        return ThemeCategory::Operator;
    default:
        return ThemeCategory::Default;
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
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

} // namespace

auto HtmlRenderer::render(const std::vector<lexer::Token>& tokens) const -> wxString {
    const auto& defaultBg = m_theme.background({});
    const auto& defaultFg = m_theme.foreground({});
    std::string output;
    output.reserve(getSizeHint() + tokens.size() * 50); // html needs lot of markup

    output += "<pre style=\"background:" + hexColour(defaultBg)
            + ";color:" + hexColour(defaultFg) + "\">";

    for (const auto& tok : tokens) {
        const auto escaped = escapeHtml(tok.text);

        if (!needsStyling(tok.kind)) {
            output += escaped;
            continue;
        }

        const auto& style = m_theme.get(tokenToCategory(tok.kind));
        std::string css = "color:" + hexColour(m_theme.foreground(style.colors.foreground))
                        + ";background:" + hexColour(m_theme.background(style.colors.background));
        if (style.bold) {
            css += ";font-weight:bold";
        }
        if (style.italic) {
            css += ";font-style:italic";
        }
        if (style.underlined) {
            css += ";text-decoration:underline";
        }

        output += "<span style=\"" + css + "\">" + escaped + "</span>";
    }

    output += "</pre>";
    return wxString::FromUTF8(output);
}

auto HtmlRenderer::decorate(const wxString& rendered) -> wxString {
    wxString output;
    output += "<!DOCTYPE html>\n<html>\n<head>\n";
    output += "<meta charset=\"utf-8\">\n";
    output += "<title>FBIde Export</title>\n";
    output += "</head>\n";
    output += "<body>" + rendered + "</body>";
    output += "</html>\n";
    return output;
}
