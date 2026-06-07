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
    case lexer::TokenKind::Keywords:
        return ThemeCategory::Keywords;
    case lexer::TokenKind::KeywordTypes:
        return ThemeCategory::KeywordTypes;
    case lexer::TokenKind::KeywordOperators:
        return ThemeCategory::KeywordOperators;
    case lexer::TokenKind::KeywordConstants:
        return ThemeCategory::KeywordConstants;
    case lexer::TokenKind::KeywordLibrary:
        return ThemeCategory::KeywordLibrary;
    case lexer::TokenKind::KeywordCustom:
        return ThemeCategory::KeywordCustom;
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

void appendEscaped(std::string& out, std::string_view text) {
    for (const auto ch : text) {
        switch (ch) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += ch;
            break;
        }
    }
}

} // namespace

auto HtmlRenderer::render(const std::vector<lexer::Token>& tokens) const -> wxString {
    const auto& defaultBg = m_theme.background({});
    const auto& defaultFg = m_theme.foreground({});
    std::string output;
    output.reserve(getSizeHint() + tokens.size() * 50); // html needs lot of markup

    output += "<pre style=\"background:";
    output += hexColour(defaultBg);
    output += ";color:";
    output += hexColour(defaultFg);
    output += "\">";

    for (const auto& tok : tokens) {
        if (!needsStyling(tok.kind)) {
            appendEscaped(output, tok.text);
            continue;
        }

        const auto& style = m_theme.get(tokenToCategory(tok.kind));
        output += "<span style=\"color:";
        output += hexColour(m_theme.foreground(style.colors.foreground));
        output += ";background:";
        output += hexColour(m_theme.background(style.colors.background));
        if (style.bold) {
            output += ";font-weight:bold";
        }
        if (style.italic) {
            output += ";font-style:italic";
        }
        if (style.underlined) {
            output += ";text-decoration:underline";
        }
        output += "\">";
        appendEscaped(output, tok.text);
        output += "</span>";
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
