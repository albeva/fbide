//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeHighlighter.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
using namespace fbide;

namespace {

/// Map a lexer token kind to the theme style category that colours it.
/// Mirrors the editor's colouring; word-shaped, structural and trivia kinds
/// fall through to `Default`.
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

/// True when `kind` carries syntax colour. Trivia and unclassified words use
/// the default text colour instead.
auto needsStyling(const lexer::TokenKind kind) -> bool {
    return kind != lexer::TokenKind::Identifier
        && kind != lexer::TokenKind::Invalid
        && kind != lexer::TokenKind::Whitespace
        && kind != lexer::TokenKind::Newline;
}

} // namespace

auto fbide::highlightCode(const std::vector<lexer::Token>& tokens, const Theme& theme)
    -> std::vector<CodeLine> {
    std::vector<CodeLine> lines;
    CodeLine current;
    const wxColour defaultFg = theme.foreground({});

    for (const auto& token : tokens) {
        // Resolve the run's colour and typeface once per token.
        wxColour colour = defaultFg;
        bool bold = false;
        bool italic = false;
        bool underlined = false;
        if (needsStyling(token.kind)) {
            const auto& entry = theme.get(tokenToCategory(token.kind));
            colour = theme.foreground(entry.colors.foreground);
            bold = entry.bold;
            italic = entry.italic;
            underlined = entry.underlined;
        }

        // A token's text may span lines (Newline tokens, and `/' '/` block
        // comments) — split it so no run ever straddles a line break.
        const std::string& text = token.text;
        std::size_t start = 0;
        while (true) {
            const std::size_t newline = text.find('\n', start);
            const std::size_t stop = newline == std::string::npos ? text.size() : newline;
            if (stop > start) {
                current.push_back({
                    .text = wxString::FromUTF8(text.data() + start, stop - start),
                    .colour = colour,
                    .bold = bold,
                    .italic = italic,
                    .underlined = underlined,
                });
            }
            if (newline == std::string::npos) {
                break;
            }
            lines.push_back(std::move(current));
            current.clear();
            start = newline + 1;
        }
    }
    lines.push_back(std::move(current));

    // md4c terminates fenced code with '\n', so the last line is usually a
    // spurious blank — drop one such trailing line.
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

CodeHighlighter::CodeHighlighter(const Value& keywordGroups)
: m_lexer(FBSciLexer::Create()) {
    lexer::configureFbWordlists(*m_lexer, keywordGroups);
}

CodeHighlighter::~CodeHighlighter() {
    if (m_lexer != nullptr) {
        m_lexer->Release();
    }
}

auto CodeHighlighter::highlight(const wxString& code, const Theme& theme) const
    -> std::vector<CodeLine> {
    const auto utf8 = code.utf8_string();

    // Lex over a headless MemoryDocument — the editor's colouring path.
    MemoryDocument doc;
    doc.Set(std::string_view { utf8.data(), utf8.size() });
    m_lexer->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);

    lexer::MemoryDocStyledSource source(doc);
    lexer::StyleLexer adapter(source);
    return highlightCode(adapter.tokenise(), theme);
}
