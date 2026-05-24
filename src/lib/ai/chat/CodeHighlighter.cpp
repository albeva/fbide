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
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "format/transformers/case/CaseTransform.hpp"
#include "format/transformers/reformat/ReFormatter.hpp"
using namespace fbide;
using namespace fbide::ai;

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

auto fbide::ai::highlightCode(const std::vector<lexer::Token>& tokens, const Theme& theme, const int tabWidth)
    -> std::vector<CodeLine> {
    const int tabStop = std::max(1, tabWidth);
    std::vector<CodeLine> lines;
    CodeLine current;
    const wxColour defaultFg = theme.foreground({});
    int column = 0; // visual column on the current line, for tab stops

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

            // Expand tabs to spaces against tab stops so indentation renders
            // (a bare '\t' has no reliable width in the painter).
            const wxString raw = wxString::FromUTF8(text.data() + start, stop - start);
            wxString segment;
            for (const wxUniChar ch : raw) {
                if (ch == '\t') {
                    const int fill = tabStop - (column % tabStop);
                    segment.Append(' ', static_cast<std::size_t>(fill));
                    column += fill;
                } else {
                    segment += ch;
                    column++;
                }
            }
            if (!segment.empty()) {
                current.push_back({
                    .text = segment,
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
            column = 0;
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

void detail::LexerReleaser::operator()(Scintilla::ILexer5* lexer) const noexcept {
    if (lexer != nullptr) {
        lexer->Release();
    }
}

CodeHighlighter::CodeHighlighter(Context& ctx)
: m_ctx(ctx)
, m_lexer(FBSciLexer::Create()) {
    lexer::configureFbWordlists(*m_lexer, m_ctx.getConfigManager().keywords().at("groups"));
}

CodeHighlighter::~CodeHighlighter() = default;

auto CodeHighlighter::highlight(const wxString& code, const bool reformat) const -> std::vector<CodeLine> {
    const auto utf8 = code.utf8_string();

    // Lex over a headless MemoryDocument — the editor's colouring path.
    MemoryDocument doc;
    doc.Set(std::string_view { utf8.data(), utf8.size() });
    m_lexer->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);

    lexer::MemoryDocStyledSource source(doc);
    lexer::StyleLexer adapter(source);
    auto tokens = adapter.tokenise();

    const int tabWidth = m_ctx.getConfigManager().config().get_or("editor.tabSize", 4);

    if (reformat) {
        // Keyword case, then re-indent + re-format to the editor's settings —
        // the Format-dialog pipeline, applied to model replies.
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        const auto& caseConfig = m_ctx.getConfigManager().keywords().at("cases");
        for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
            const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
            cases[idx] = CaseMode::parse(caseConfig.get_or(key, "None").ToStdString())
                             .value_or(CaseMode::None);
        }
        tokens = CaseTransform(cases).apply(tokens);

        reformat::ReFormatter formatter(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(tabWidth),
            .reIndent = true,
            .reFormat = true,
        });
        tokens = formatter.apply(tokens);
    }

    return highlightCode(tokens, m_ctx.getTheme(), tabWidth);
}
