//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeTransformer.hpp"
#include "AutoIndent.hpp"
#include "Editor.hpp"
#include "analyses/lexer/Lexer.hpp"
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

auto buildKeywordGroups(Context& ctx) -> std::vector<lexer::KeywordGroup> {
    const auto& keywords = ctx.getConfigManager().keywords();
    std::vector<lexer::KeywordGroup> groups;
    groups.reserve(kThemeKeywordCategories.size());
    for (const auto cat : kThemeKeywordCategories) {
        groups.push_back({
            keywords.get_or(wxString(getThemeCategoryName(cat)), ""),
            lexer::tokenKindFor(cat),
            lexer::scopeFor(cat),
        });
    }
    return groups;
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

    const auto groups = buildKeywordGroups(m_ctx);
    m_lexer = std::make_unique<lexer::Lexer>(groups);
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
    // Lex the current line up to wordEnd so the last token corresponds
    // to the word in question. This lets the lexer tell us if the word
    // sits inside a string / comment — token kind won't be a keyword
    // group and we leave it alone.
    const int line = editor.LineFromPosition(wordStart);
    const int lineStart = editor.PositionFromLine(line);
    const auto prefix = editor.GetTextRange(lineStart, wordEnd);
    const auto utf8 = prefix.utf8_str();
    m_lexer->tokeniseInto(utf8.data(), m_tokenBuffer);

    if (m_tokenBuffer.empty()) {
        return;
    }

    const auto targetOffset = static_cast<std::size_t>(wordStart - lineStart);
    std::size_t offset = 0;
    const lexer::Token* match = nullptr;
    for (const auto& t : m_tokenBuffer) {
        if (offset == targetOffset && !t.text.empty()) {
            match = &t;
            break;
        }
        if (offset > targetOffset) {
            break;
        }
        offset += t.text.size();
    }
    if (match == nullptr || !lexer::isKeywordToken(match->kind)) {
        return;
    }

    const auto original = std::string { match->text };
    const auto transformed = m_keywordCase.apply(original);
    if (transformed == original) {
        return;
    }

    const int caretBefore = editor.GetCurrentPos();
    editor.BeginUndoAction();
    editor.SetTargetRange(wordStart, wordEnd);
    editor.ReplaceTarget(wxString::FromUTF8(transformed.c_str(), transformed.size()));
    // Length is preserved for ASCII case transforms, but pin the cursor
    // back where it was for safety.
    editor.GotoPos(caretBefore);
    editor.EndUndoAction();

    // Force synchronous restyle of the changed line so the keyword colour
    // re-applies immediately — without this the buffer briefly shows the
    // word as plain text until Scintilla's deferred lex runs.
    editor.Colourise(lineStart, editor.GetLineEndPosition(line));
}

void CodeTransformer::onTextInserted(Editor& editor, const int pos, const int length) {
    if (m_inAction) {
        return;
    }
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

    const int firstLine = editor.LineFromPosition(rangeStart);
    const int lastLine = editor.LineFromPosition(rangeEnd);
    const int caretBefore = editor.GetCurrentPos();

    editor.BeginUndoAction();
    for (int line = firstLine; line <= lastLine; line++) {
        const int lineStart = editor.PositionFromLine(line);
        const int lineEnd = editor.GetLineEndPosition(line);
        const auto text = editor.GetTextRange(lineStart, lineEnd);
        const auto utf8 = text.utf8_str();
        m_lexer->tokeniseInto(utf8.data(), m_tokenBuffer);

        std::size_t offset = 0;
        for (const auto& t : m_tokenBuffer) {
            const auto len = t.text.size();
            if (lexer::isKeywordToken(t.kind)) {
                const int absStart = lineStart + static_cast<int>(offset);
                const int absEnd = absStart + static_cast<int>(len);
                if (absEnd > rangeStart && absStart < rangeEnd) {
                    auto cased = m_keywordCase.apply(std::string { t.text });
                    if (cased != t.text) {
                        editor.SetTargetRange(absStart, absEnd);
                        editor.ReplaceTarget(wxString::FromUTF8(cased.c_str(), cased.size()));
                    }
                }
            }
            offset += len;
        }
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
