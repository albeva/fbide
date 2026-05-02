//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/Theme.hpp"
#include "document/DocumentType.hpp"

namespace fbide {
class Context;
class CodeTransformer;

/**
 * The text-editing widget — a `wxStyledTextCtrl` (Scintilla) subclass.
 * One instance per `Document` plus one per Format-dialog preview pane.
 *
 * **Owns:** internal Scintilla state. `Editor` itself is wx-parented
 * (notebook page or preview pane).
 * **Threading:** UI thread only.
 * **Per-DocumentType behaviour:** `applyTheme()` dispatches to a
 * per-type method (`applyFreebasicTheme`, `applyHtmlTheme`, ...) which
 * configures the lexer and style ids.
 *
 * See @ref editor.
 */
class Editor final : public wxStyledTextCtrl {
public:
    NO_COPY_AND_MOVE(Editor)

    /**
     * Construct an Editor as a child of `parent`.
     *
     * @param parent      wx-owning parent (notebook page or preview pane).
     * @param ctx         Application context.
     * @param transformer Shared on-type transformer. May be `nullptr`
     *                    for preview editors — when non-null, `Editor`
     *                    routes `EVT_STC_CHARADDED` and `applySettings`
     *                    into it.
     * @param type        Initial `DocumentType` (drives lexer + theme dispatch).
     * @param preview     When true, hides every margin and decoration.
     */
    Editor(
        wxWindow* parent,
        Context& ctx,
        CodeTransformer* transformer,
        DocumentType type = DocumentType::FreeBASIC,
        bool preview = false
    );

    /// Apply theme and settings from context.
    void applySettings();

    /// Get document type.
    [[nodiscard]] auto getDocType() const -> DocumentType { return m_docType; }

    /// Change document type
    void setDocType(DocumentType type);

    /// Select current line.
    void selectLine();

    /// Get word under the cursor, or selected text if any.
    [[nodiscard]] auto getWordAtCursor() -> wxString;

    /// Find next occurrence of text. Returns true if found.
    auto findNext(const wxString& text, int flags, bool forward = true) -> bool;

    /// Replace current selection if it matches, then find next. Returns true if replaced.
    auto replaceNext(const wxString& findText, const wxString& replaceText, int flags) -> bool;

    /// Replace all occurrences. Returns the number of replacements made.
    auto replaceAll(const wxString& findText, const wxString& replaceText, int flags) -> int;

    /// Go to line (1-based). Supports "line:col" and "e" for end.
    void gotoLine(const wxString& input);

    /// Navigate to a line (1-based), centering it in the viewport.
    void navigateToLine(int line);

    /// Comment selected lines (prepend ').
    void commentSelection();

    /// Uncomment selected lines (strip leading ' or REM).
    void uncommentSelection();

    /// Update the statusbar with current cursor position.
    void updateStatusBar() const;

    /// Enable / disable code transforms (e.g. during loading)
    void disableTransforms(bool state);

private:
    /// Margin click — toggle folds on the fold margin.
    void onMarginClick(wxStyledTextEvent& event);
    /// Buffer modified — restart intellisense timer; route bulk inserts.
    void onModified(wxStyledTextEvent& event);
    /// Coalesced "something changed" — status bar, brace match, sync edit cmds.
    void onUpdateUI(wxStyledTextEvent& event);
    /// Deferred follow-up after `onUpdateUI` — runs once per UI tick.
    void postUpdateUI();
    /// Zoom event — bump the line-number margin width.
    void onZoom(wxStyledTextEvent& event);
    /// Single-char insert — drives `CodeTransformer` on-type pipeline.
    void onCharAdded(wxStyledTextEvent& event);
    /// Editor gained focus — refresh edit-command masks.
    void onFocus(wxFocusEvent& event);
    /// Intellisense timer fire — submit current text to the worker.
    void onIntellisenseTimer(wxTimerEvent& event);
    /// Hotspot click — Ctrl+click on `#include` jumps to the included file.
    void onHotSpotClick(wxStyledTextEvent& event);
    /// Key down — toggle hotspot styling when Ctrl is pressed.
    void onKeyDown(wxKeyEvent& event);
    /// Key up — toggle hotspot styling off when Ctrl is released.
    void onKeyUp(wxKeyEvent& event);
    /// Editor lost focus — clear hotspot styling so it doesn't linger.
    void onKillFocus(wxFocusEvent& event);
    /// Toggle Scintilla hotspot style on Preprocessor styles.
    void setIncludeHotspots(bool active);
    /// Recompute brace match for the current caret position.
    void updateBraceMatch();
    /// Reapply editor settings (tab size, EOL visibility, etc.) from config.
    void applyEditorSettings();
    /// Configure fold margins + marker colors from the active theme.
    void defineFoldMargins();
    /// Apply theme via the per-`DocumentType` dispatch.
    void applyTheme();
    /// Apply a single theme `Entry` to the given Scintilla style id.
    void applyStyle(int stcId, const Theme::Entry& style, const Theme& theme);
    /// Apply foreground/background colors to the given Scintilla style id.
    void applyColors(int stcId, const Theme::Colors& colors, const Theme& theme);
    /// Theme dispatch for FreeBASIC documents (custom lexer + every category).
    void applyFreebasicTheme();
    /// Theme dispatch for HTML documents (built-in wxSTC HTML lexer).
    void applyHtmlTheme();
    /// Theme dispatch for properties / `.ini` documents.
    void applyPropertiesTheme();
    /// Theme dispatch for plain-text documents (no lexer).
    void applyTextTheme();
    /// Resize the line-number margin to fit the current line count + zoom.
    void updateLineNumberMarginWidth();

    Context& m_ctx;                  ///< Application context.
    CodeTransformer* m_transformer;  ///< Shared on-type transformer (nullable in preview).
    DocumentType m_docType;          ///< Current document type — drives theme dispatch.
    wxFont m_font;                   ///< Editor font.
    bool m_preview;                  ///< True when this is a Format-dialog preview pane.
    bool m_insertHandled = false;    ///< Latch to dedupe single-char vs multi-char insert paths.
    bool m_editorLocked = false;     ///< Set during load/reload to suppress on-type transforms.
    bool m_includeHotspotsActive = false; ///< True when Ctrl is held and PP styles show hotspot cursor.
    int m_lastCaretPos = 0;          ///< Caret position from previous `onUpdateUI` — backs `onCaretMoved`.
    bool m_callPostUpdate = false;   ///< Latch — triggers `postUpdateUI` on the next tick.
    /// Restart on each text-changing modify event; on fire submits a
    /// snapshot to DocumentManager::submitIntellisense.
    wxTimer m_intellisenseTimer;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
