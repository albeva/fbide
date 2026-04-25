//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/ThemeCategory.hpp"
#include "format/transformers/case/CaseTransform.hpp"

namespace fbide {
class Context;
class Editor;

/// Per-Editor driver for on-type code transforms:
///   - Auto-indent on Enter (wraps indent::decide).
///   - Keyword case normalisation on word boundary.
///   - Auto-insert matching closing keyword for block openers.
///
/// Reads style bytes that FBSciLexer already published into the editor's
/// document — never runs its own lex of the editor content. Field-access
/// suppression (words after `.` or `->`, including across `_` line
/// continuation) and asm/comment/string contexts are inherited for free.
class CodeTransformer final {
public:
    NO_COPY_AND_MOVE(CodeTransformer)

    explicit CodeTransformer(Context& ctx);
    ~CodeTransformer();

    /// Reload from config (`editor.autoIndent`, `editor.keywordCase`) and
    /// rebuild the internal Lexer from the current keyword groups.
    void applySettings();

    /// Main entry from Editor's EVT_STC_CHARADDED handler.
    void onCharAdded(Editor& editor, int ch);

    /// Called from Editor's EVT_STC_UPDATEUI handler when the caret
    /// changes position. If the caret left an unfinished word (a word
    /// char run ending at `oldPos`), case-normalise that word.
    void onCaretMoved(Editor& editor, int oldPos);

    /// Called from Editor's EVT_STC_MODIFIED handler when a bulk insert
    /// happens (paste, drop, ...). Runs case normalisation across the
    /// pasted range — does NOT touch indentation.
    void onTextInserted(Editor& editor, int pos, int length);

    /// Enable / disable code transformer
    void enable(bool state);

private:
    void applyIndentAndCloser(Editor& editor);
    void applyWordCase(Editor& editor);
    void transformWordInRange(Editor& editor, int wordStart, int wordEnd);
    void transformRange(Editor& editor, int rangeStart, int rangeEnd);
    [[nodiscard]] auto renderCloser(std::span<const std::string_view> words) const -> wxString;

    Context& m_ctx;
    bool m_autoIndent = true;
    bool m_transformKeywords = true;
    std::array<CaseMode, kThemeKeywordGroupsCount> m_keywordCases {};
};

} // namespace fbide
