//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <span>
#include <string_view>
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
    void onCaretMoved(Editor& editor, int oldPos, int newPos);

    /// Called from Editor's EVT_STC_MODIFIED handler when a bulk insert
    /// happens (paste, drop, ...). Runs case normalisation across the
    /// pasted range — does NOT touch indentation.
    void onTextInserted(Editor& editor, int pos, int length);

    /// Suppress paste handling for the duration of a programmatic bulk
    /// insert (e.g. file load via SetText). Use the RAII helper below.
    class Suspend {
    public:
        explicit Suspend(CodeTransformer& t)
        : m_t(t)
        , m_prev(t.m_inAction) { t.m_inAction = true; }
        ~Suspend() { m_t.m_inAction = m_prev; }
        Suspend(const Suspend&) = delete;
        Suspend& operator=(const Suspend&) = delete;
    private:
        CodeTransformer& m_t;
        bool m_prev;
    };

private:
    void applyIndentAndCloser(Editor& editor);
    void applyWordCase(Editor& editor, int ch);
    /// Transform the word at `[wordStart, wordEnd)` if it's a keyword.
    /// Assumes the current caret sits outside that range — callers must
    /// uphold this to keep the cursor in place after ReplaceTarget.
    void transformWordInRange(Editor& editor, int wordStart, int wordEnd);
    /// Re-case every keyword token whose range intersects
    /// `[rangeStart, rangeEnd)`. Used for paste / drop.
    void transformRange(Editor& editor, int rangeStart, int rangeEnd);
    [[nodiscard]] auto renderCloser(std::span<const std::string_view> words) const -> wxString;

    Context& m_ctx;
    bool m_autoIndent = true;
    bool m_inAction = false; // re-entry guard for STC modification events
    /// Set by onTextInserted when external text is added; consumed by the
    /// next onCaretMoved. Distinguishes "caret moved because user typed a
    /// char" (handled by onCharAdded) from genuine navigation (arrow keys,
    /// mouse). Without this, every keystroke looks like a navigation away
    /// from a partial word and triggers premature case transforms.
    bool m_pendingTextChange = false;
    CaseMode m_keywordCase = CaseMode::Lower;

    friend struct ActionGuard;
};

} // namespace fbide
