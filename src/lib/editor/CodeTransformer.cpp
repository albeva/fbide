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

namespace fbide {

struct ActionGuard {
    CodeTransformer& self;
    bool prev;
    explicit ActionGuard(CodeTransformer& s)
    : self(s)
    , prev(s.m_inAction) { self.m_inAction = true; }
    ~ActionGuard() { self.m_inAction = prev; }
    ActionGuard(const ActionGuard&) = delete;
    ActionGuard& operator=(const ActionGuard&) = delete;
};

} // namespace fbide

CodeTransformer::CodeTransformer(Context& ctx)
: m_ctx(ctx) {
    applySettings();
}

CodeTransformer::~CodeTransformer() = default;

void CodeTransformer::applySettings() {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    m_autoIndent = editor.get_or("autoIndent", true);
    m_keywordCase = CaseMode::parse(editor.get_or("keywordCase", "Lower").ToStdString())
                        .value_or(CaseMode::Lower);
}

void CodeTransformer::onCharAdded(Editor& editor, const int ch) {
    ActionGuard guard(*this);
    editor.BeginUndoAction();
    // Word-boundary trigger for case normalisation: anything that is NOT
    // a word character ends the previous identifier. Run this first so
    // the boundary char (including newline) finalises the previous word
    // before we touch indentation.
    if (!isWordChar(ch) && m_keywordCase != CaseMode::None) {
        applyWordCase(editor, ch);
    }

    if (ch == '\n' && m_autoIndent) {
        applyIndentAndCloser(editor);
    }
    editor.EndUndoAction();
}

void CodeTransformer::applyIndentAndCloser(Editor& editor) {
    const int currLine = editor.GetCurrentLine();
    if (currLine == 0) {
        return;
    }
    const int prevLine = currLine - 1;

    const auto decision = indent::decide(editor.GetLine(prevLine));
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

void CodeTransformer::applyWordCase(Editor& editor, int /*ch*/) {
    const int caretPos = editor.GetCurrentPos();
    if (caretPos < 2) {
        // Need at least the boundary char + one word char behind.
        return;
    }

    // The boundary char sits at caretPos - 1; the word ends just before it.
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

void CodeTransformer::onCaretMoved(Editor& editor, const int oldPos, const int newPos) {
    if (m_inAction) {
        return;
    }
    // Caret moved because the user just typed a char — let onCharAdded
    // handle word boundaries when (and if) the typed char is one. Skip
    // here so partial words don't get prematurely transformed.
    if (m_pendingTextChange) {
        m_pendingTextChange = false;
        return;
    }
    if (m_keywordCase == CaseMode::None) {
        return;
    }
    if (oldPos <= 0) {
        return;
    }
    ActionGuard guard(*this);

    // Walk back from oldPos over word chars to find the run that the
    // caret was attached to. If oldPos didn't sit at the end of a word
    // run there is nothing to transform.
    int wordStart = oldPos;
    while (wordStart > 0 && isWordChar(editor.GetCharAt(wordStart - 1))) {
        wordStart--;
    }
    const int wordEnd = oldPos;
    if (wordStart == wordEnd) {
        return;
    }

    // Caret still sits inside the same word — user is editing it. Don't
    // transform yet; wait until they leave.
    if (newPos > wordStart && newPos < wordEnd) {
        return;
    }
    if (newPos == wordEnd) {
        // Caret didn't actually leave the word's trailing edge.
        return;
    }
    transformWordInRange(editor, wordStart, wordEnd);
}

void CodeTransformer::transformWordInRange(Editor& editor, const int wordStart, const int wordEnd) {
    // Hot path: just read the style FBSciLexer already published. No re-lex.
    // FBSciLexer's field-access state (across `_` continuation), asm scoping,
    // string/comment context all come for free — anything not styled as a
    // keyword group is left alone.
    const int line = editor.LineFromPosition(wordStart);
    const int lineStart = editor.PositionFromLine(line);
    // Force STC to lex past the boundary char (which sits at caretPos-1) so
    // FBSciLexer commits the keyword style for the just-finished word.
    // GetCurrentPos is one past the boundary so it's safely included whether
    // Colourise's end is inclusive or exclusive.
    editor.Colourise(lineStart, editor.GetCurrentPos());
    const auto style = static_cast<ThemeCategory>(editor.GetStyleAt(wordStart));
    if (!isKeywordCategory(style)) {
        return;
    }

    const auto wxOriginal = editor.GetTextRange(wordStart, wordEnd);
    const auto utf8 = wxOriginal.utf8_str();
    const std::string original { utf8.data(), utf8.length() };
    const auto transformed = m_keywordCase.apply(original);
    if (transformed == original) {
        return;
    }

    const int caretBefore = editor.GetCurrentPos();
    editor.BeginUndoAction();
    editor.SetTargetRange(wordStart, wordEnd);
    editor.ReplaceTarget(wxString::FromUTF8(transformed.c_str(), transformed.size()));
    editor.GotoPos(caretBefore);
    editor.EndUndoAction();

    // Force synchronous restyle of the changed line so the keyword colour
    // re-applies immediately.
    editor.Colourise(lineStart, editor.GetLineEndPosition(line));
}

void CodeTransformer::onTextInserted(Editor& editor, const int pos, const int length) {
    if (m_inAction) {
        return;
    }
    // External text change — flag for the upcoming onCaretMoved so it
    // doesn't mistake a typing-induced caret bump for a navigation away.
    m_pendingTextChange = true;
    if (m_keywordCase == CaseMode::None) {
        return;
    }
    if (length <= 1) {
        // Single-char insert is normal typing, handled by onCharAdded.
        return;
    }
    transformRange(editor, pos, pos + length);
}

void CodeTransformer::transformRange(Editor& editor, const int rangeStart, const int rangeEnd) {
    ActionGuard guard(*this);

    // Force STC to style up to rangeEnd so StyleLexer reads accurate styles.
    editor.Colourise(0, rangeEnd);

    lexer::WxStcStyledSource src(editor);
    lexer::StyleLexer adapter(src);
    const auto tokens = adapter.tokenise();

    const int caretBefore = editor.GetCurrentPos();
    editor.BeginUndoAction();
    Sci_PositionU pos = 0;
    for (const auto& t : tokens) {
        const auto len = static_cast<int>(t.text.size());
        const int absStart = static_cast<int>(pos);
        const int absEnd = absStart + len;
        if (lexer::isKeywordToken(t.kind) && absEnd > rangeStart && absStart < rangeEnd) {
            auto cased = m_keywordCase.apply(std::string { t.text });
            if (cased != t.text) {
                editor.SetTargetRange(absStart, absEnd);
                editor.ReplaceTarget(wxString::FromUTF8(cased.c_str(), cased.size()));
            }
        }
        pos += t.text.size();
    }
    editor.GotoPos(caretBefore);
    editor.EndUndoAction();
}

auto CodeTransformer::renderCloser(const std::span<const std::string_view> words) const -> wxString {
    // `None` is a no-op for user-typed keywords, but closers still need a
    // concrete case — default to lowercase so the output is at least
    // consistent rather than raw placeholder.
    const auto rule = m_keywordCase == CaseMode::None ? CaseMode { CaseMode::Lower } : m_keywordCase;

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
