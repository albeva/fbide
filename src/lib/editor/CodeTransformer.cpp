//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeTransformer.hpp"
#include "AutoIndent.hpp"
#include "Editor.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/Token.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
using namespace fbide;

namespace {

auto isWordChar(const wxUniChar ch) -> bool {
    const auto c = ch.GetValue();
    return (c >= '0' && c <= '9')
        || (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || c == '_';
}

} // namespace

CodeTransformer::CodeTransformer(Context& ctx)
: m_ctx(ctx) {
    applySettings();
}

CodeTransformer::~CodeTransformer() = default;

void CodeTransformer::applySettings() {
    enable(true);
    const auto& cases = m_ctx.getConfigManager().keywords().at("cases");
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
        m_keywordCases[idx] = CaseMode::parse(cases.get_or(key, "None").ToStdString())
                                  .value_or(CaseMode::None);
    }
}

// ===========================================================================
// Action handlers
// ===========================================================================

void CodeTransformer::onCharAdded(Editor& editor, const int ch) {
    if (not m_transformKeywords || isWordChar(ch)) {
        return;
    }

    editor.SetUndoCollection(false);
    applyWordCase(editor);
    if (ch == '\n' && m_autoIndent) {
        applyIndentAndCloser(editor);
    }
    editor.SetUndoCollection(true);
}

void CodeTransformer::onCaretMoved(Editor& editor, const int oldPos) {
    if (!m_transformKeywords || oldPos <= 0) {
        return;
    }

    int wordStart = oldPos;
    while (wordStart > 0 && isWordChar(editor.GetCharAt(wordStart - 1))) {
        wordStart--;
    }
    const int wordEnd = oldPos;
    if (wordStart == wordEnd) {
        return;
    }

    editor.SetUndoCollection(false);
    transformWordInRange(editor, wordStart, wordEnd);
    editor.SetUndoCollection(true);
}

void CodeTransformer::onTextInserted(Editor& editor, const int pos, const int length) {
    if (!m_transformKeywords) {
        return;
    }
    transformRange(editor, pos, pos + length);
}

void CodeTransformer::enable(const bool state) {
    if (state) {
        const auto& editor = m_ctx.getConfigManager().config().at("editor");
        m_autoIndent = editor.get_or("autoIndent", true);
        m_transformKeywords = editor.get_or("transformKeywords", true);
    } else {
        m_autoIndent = false;
        m_transformKeywords = false;
    }
}

// ===========================================================================
// Internal transform handlers
// ===========================================================================

void CodeTransformer::applyIndentAndCloser(Editor& editor) {
    const int currLine = editor.GetCurrentLine();
    if (currLine == 0) {
        return;
    }
    const int prevLine = currLine - 1;

    const auto decision = indent::Decision::decide(editor, prevLine);
    const int tabSize = editor.GetIndent();

    int prevIndent = editor.GetLineIndentation(prevLine);
    if (decision.dedentPrev) {
        prevIndent = std::max(0, prevIndent - tabSize);
        editor.SetLineIndentation(prevLine, prevIndent);
    }
    const int newIndent = std::max(0, prevIndent + decision.deltaLevels * tabSize);
    editor.SetLineIndentation(currLine, newIndent);
    editor.GotoPos(editor.GetLineEndPosition(currLine));

    if (!decision.closerKeywords.empty()) {
        const int caretPos = editor.GetLineEndPosition(currLine);
        editor.InsertText(caretPos, "\n" + renderCloser(decision.closerKeywords));
        editor.SetLineIndentation(currLine + 1, prevIndent);
        editor.GotoPos(editor.GetLineEndPosition(currLine));
    }
}

void CodeTransformer::applyWordCase(Editor& editor) {
    const int caretPos = editor.GetCurrentPos();
    if (caretPos < 2) {
        return;
    }

    const int wordEnd = caretPos - 1;
    int wordStart = wordEnd;
    while (wordStart > 0 && isWordChar(editor.GetCharAt(wordStart - 1))) {
        wordStart--;
    }

    if (wordStart == wordEnd) {
        return;
    }

    transformWordInRange(editor, wordStart, wordEnd);
}

void CodeTransformer::transformWordInRange(Editor& editor, const int wordStart, const int wordEnd) {
    const auto style = static_cast<ThemeCategory>(editor.GetStyleAt(wordStart));
    if (!isKeywordCategory(style)) {
        return;
    }

    const auto caseMode = m_keywordCases[indexOfKeywordGroup(style)];
    if (caseMode == CaseMode::None) {
        return;
    }

    const auto original = editor.GetTextRange(wordStart, wordEnd);
    const auto replace = caseMode.apply(original);
    if (original == replace) {
        return;
    }

    editor.SetTargetRange(wordStart, wordEnd);
    editor.ReplaceTarget(replace);
    editor.StartStyling(wordStart);
    editor.SetStyling(wordEnd - wordStart, +style);
}

void CodeTransformer::transformRange(Editor& editor, const int rangeStart, const int rangeEnd) {
    // Force STC to re-style
    editor.Colourise(0, rangeEnd);

    // Tokenize the inserted range
    lexer::WxStcStyledSource src(editor);
    lexer::StyleLexer adapter(src);
    const auto tokens = adapter.tokenise({ rangeStart, rangeEnd });

    editor.SetUndoCollection(false);

    Sci_PositionU pos = static_cast<Sci_PositionU>(rangeStart);
    for (const auto& tkn : tokens) {
        const auto len = static_cast<int>(tkn.text.size());
        const int start = static_cast<int>(pos);
        const int end = start + len;

        if (lexer::isKeywordToken(tkn.kind)) {
            const auto caseMode = m_keywordCases[indexOfKeywordGroup(tkn.style)];
            if (caseMode != CaseMode::None) {
                auto cased = caseMode.apply(tkn.text);
                if (cased != tkn.text) {
                    editor.SetTargetRange(start, end);
                    editor.ReplaceTarget(wxString::FromUTF8(cased));
                    editor.StartStyling(start);
                    editor.SetStyling(end - start, +tkn.style);
                }
            }
        }
        pos += tkn.text.size();
    }
    editor.SetUndoCollection(true);
}

auto CodeTransformer::renderCloser(const std::span<const std::string_view> words) const -> wxString {
    // Closers are Keyword1 words (`end`, `if`, `sub`, ...). Use that group's
    // configured case rule. `None` falls back to lowercase so output is at
    // least consistent rather than raw placeholder.
    const auto kw1Mode = m_keywordCases[indexOfKeywordGroup(ThemeCategory::Keywords)];
    const auto rule = kw1Mode == CaseMode::None ? CaseMode { CaseMode::Lower } : kw1Mode;

    wxString out;
    for (std::size_t i = 0; i < words.size(); i++) {
        if (i > 0) {
            out += " ";
        }
        auto cased = rule.apply(std::string { words[i] });
        out += wxString::FromUTF8(cased.c_str(), cased.size());
    }
    return out;
}
