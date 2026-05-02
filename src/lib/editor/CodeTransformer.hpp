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

    /// Construct and seed from `editor.*` config keys.
    explicit CodeTransformer(Context& ctx);
    /// Destroy, releasing the internal Lexer.
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
    /// Auto-indent the new line and (if appropriate) emit a matching closer.
    void applyIndentAndCloser(Editor& editor);
    /// Run the keyword-case transform on the word the caret just left.
    void applyWordCase(Editor& editor);
    /// Transform a single word range to its configured case.
    void transformWordInRange(Editor& editor, int wordStart, int wordEnd);
    /// Transform every word in `[rangeStart, rangeEnd)` to its configured case.
    void transformRange(Editor& editor, int rangeStart, int rangeEnd);
    /// Render `words` to the canonical closer text using the active editor's case rules.
    [[nodiscard]] auto renderCloser(std::span<const std::string_view> words) const -> wxString;

    Context& m_ctx;                  ///< Application context.
    bool m_autoIndent = true;        ///< Master toggle for auto-indent.
    bool m_transformKeywords = true; ///< Master toggle for keyword case transform.
    std::array<CaseMode, kThemeKeywordGroupsCount> m_keywordCases {}; ///< Per-keyword-group case mode.
};

} // namespace fbide
