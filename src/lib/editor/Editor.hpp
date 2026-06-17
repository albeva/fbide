//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "LineHistory.hpp"
#include "config/Theme.hpp"
#include "document/DocumentType.hpp"

namespace fbide {
class CodeTransformer;
class ConfigManager;
class DocumentManager;
class Theme;
class UIManager;

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
        ConfigManager& configManager,
        Theme& theme,
        DocumentManager* documentManager,
        UIManager* uiManager,
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

    /// The whole document as an owned UTF-8 byte string, copied straight from
    /// Scintilla's buffer (already UTF-8) — no wxString decode/re-encode.
    [[nodiscard]] auto utf8Text() -> std::string;

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

    /// Push the focused-document UI state (Compile/Run menu enables, etc.)
    /// based on the current document type. Called on focus and whenever
    /// the type changes mid-session.
    void updateDocumentState() const;

    /// Enable / disable code transforms (e.g. during loading)
    void disableTransforms(bool state);

    /// Configure the change-tracking margin — width, mask and the two
    /// markers (Added / Modified) that the upcoming `LineHistory` will
    /// drive. Reapplied from `applySettings` so theme changes pick up.
    void defineChangesMargin();

    /// Scintilla marker numbers used by the change-tracking margin.
    /// Public so tests (and any future overlay) can query a line's
    /// state via `MarkerGet(line) & (1 << kAddedMarker)`.
    static constexpr int kAddedMarker = 0;
    static constexpr int kModifiedMarker = 1;

private:
    /// Selection captured before a line-prefix edit (comment/uncomment) so it
    /// can be restored afterwards, preserving direction (anchor vs caret) and
    /// type (stream vs rectangular block).
    struct SavedSelection {
        bool rectangular;
        int anchor;
        int caret;
        int anchorLine;
        int caretLine;
    };
    /// Snapshot the current selection.
    [[nodiscard]] auto captureSelection() const -> SavedSelection;
    /// Re-apply a snapshot with already-shifted endpoints, keeping its type and
    /// direction.
    void applySelection(const SavedSelection& saved, int newAnchor, int newCaret);

    /// Margin click — toggle folds on the fold margin.
    void onMarginClick(wxStyledTextEvent& event);
    /// Buffer modified — restart intellisense timer; route bulk inserts;
    /// feed the change-tracker.
    void onModified(wxStyledTextEvent& event);
    /// Save-point reached — fired when the document returns to its
    /// saved state (either by save or by undo to clean). Re-snapshots
    /// the change tracker and clears every change marker.
    void onSavePointReached(wxStyledTextEvent& event);
    /// Apply an insert / delete from `onModified` to `m_lineHistory` and
    /// refresh the change-margin markers on the affected line range.
    void updateChangeTracking(wxStyledTextEvent& event);
    /// Re-mark lines `[from, to]` from `m_lineHistory::stateOf`. Clears
    /// any stale marker on those lines first.
    void remarkChangedLines(int from, int to);
    /// Snapshot the current document as the new "clean" baseline and
    /// clear every change marker. Called from `onSavePointReached`.
    void resnapshotChangeTracker();
    /// Coalesced "something changed" — status bar, brace match, sync edit cmds.
    void onUpdateUI(wxStyledTextEvent& event);
    /// Deferred follow-up after `onUpdateUI` — runs once per UI tick.
    void postUpdateUI();
    /// Highlight every occurrence of the identifier under the caret — only when
    /// the selection is empty, the caret sits inside an identifier (not a keyword),
    /// and the tick was a caret move rather than a typing edit. From `postUpdateUI`.
    void updateOccurrenceHighlight();
    /// Remove both occurrence-highlight indicators and reset the cache.
    void clearOccurrenceHighlight();
    /// Identifier eligible for occurrence highlight — the word under the caret, or
    /// the selection when it spans exactly one identifier. Empty on a keyword, a
    /// partial/multi-token selection, or a word shorter than two characters.
    auto occurrenceWordAtCaret() -> wxString;
    /// Paint both occurrence indicators over every whole-word match of `word`.
    void fillOccurrences(const wxString& word);
    /// Highlight the matching opener/closer keyword when the caret is on a
    /// block keyword (for/next, sub/end sub, ...), or the enclosing procedure
    /// when the caret is on `Return`. Reads the document's SymbolTable scope tree.
    void updateKeywordMatch();
    /// Remove the keyword-match indicator.
    void clearKeywordMatch();
    /// Configure an indicator pair (background box + TEXTFORE) with the
    /// WordHighlight style. Shared by the occurrence and keyword-match
    /// highlights so they render identically.
    void configureMatchIndicators(int bgIndic, int textIndic);
    /// Coalesced transformer pass for a burst of text inserts — see `onModified`.
    void flushPendingInsert();
    /// Zoom event — bump the line-number margin width.
    void onZoom(wxStyledTextEvent& event);
    /// Single-char insert — drives `CodeTransformer` on-type pipeline.
    void onCharAdded(wxStyledTextEvent& event);
    /// Show the symbol/keyword completion popup when the caret is at a
    /// statement start. `manual` (Ctrl+Space) shows even with no partial word.
    void maybeShowCompletion(bool manual = false);
    /// Rebuild the shared keyword-completion list (Library / Constants / Preprocessor /
    /// Custom groups) from config. Called when editor settings are applied.
    void rebuildKeywordCompletions();
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
    /// Mouse click — counts as navigation, so it re-enables occurrence
    /// highlighting after it was suppressed by typing.
    void onLeftDown(wxMouseEvent& event);
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
    /// Load correct lexer for the given file type
    void loadLexer();
    /// Apply theme for the lexer
    void loadLexerTheme();
    /// Theme dispatch for FreeBASIC documents (custom lexer + every category).
    void applyFreebasicTheme();
    /// Theme dispatch for HTML documents (built-in wxSTC HTML lexer).
    void applyHtmlTheme();
    /// Theme dispatch for properties / `.ini` documents.
    void applyPropertiesTheme();
    /// Theme dispatch for Markdown documents (built-in wxSTC Markdown lexer).
    void applyMarkdownTheme();
    /// Theme dispatch for Windows batch files (built-in wxSTC Batch lexer).
    void applyBatchTheme();
    /// Theme dispatch for shell / bash scripts (built-in wxSTC Bash lexer).
    void applyBashTheme();
    /// Theme dispatch for Makefiles (built-in wxSTC Makefile lexer).
    void applyMakefileTheme();
    /// Theme dispatch for JSON documents (built-in wxSTC JSON lexer).
    void applyJsonTheme();
    /// Theme dispatch for CSS documents (built-in wxSTC CSS lexer).
    void applyCssTheme();
    /// Theme dispatch for plain-text documents (no lexer).
    void applyTextTheme();
    /// Resize the line-number margin to fit the current line count + zoom.
    void updateLineNumberMarginWidth();

    ConfigManager& m_configManager;       ///< Config source — settings, keywords, theme entries.
    Theme& m_theme;                       ///< Active editor theme.
    DocumentManager* m_documentManager;   ///< Optional — null in standalone/test contexts.
    UIManager* m_uiManager;               ///< Optional — null in standalone/test contexts.
    CodeTransformer* m_transformer;       ///< Shared on-type transformer (nullable in preview).
    DocumentType m_docType;               ///< Current document type — drives theme dispatch.
    wxFont m_font;                        ///< Editor font.
    bool m_preview;                       ///< True when this is a Format-dialog preview pane.
    bool m_insertHandled = false;         ///< Latch to dedupe single-char vs multi-char insert paths.
    bool m_editorLocked = false;          ///< Set during load/reload to suppress on-type transforms.
    bool m_includeHotspotsActive = false; ///< True when Ctrl is held and PP styles show hotspot cursor.
    int m_lastCaretPos = 0;               ///< Caret position from previous `onUpdateUI` — backs `onCaretMoved`.
    bool m_callPostUpdate = false;        ///< Latch — triggers `postUpdateUI` on the next tick.
    wxString m_lastHighlightedWord;       ///< Identifier last painted by the occurrence highlighter; empty when none.
    bool m_matchSuppressed = false;       ///< Occurrence + keyword-match highlighting off after a text edit until the next navigation (arrow / click).
    std::vector<wxString> m_completionItems; ///< Reusable assembled candidate list for the popup.
    wxString m_completionList;               ///< Reusable space-joined item list for AutoCompShow.
    std::vector<wxString> m_localVariables;  ///< Per-caret bucket: params + in-scope locals.
    std::vector<wxString> m_localSymbols;    ///< Per-caret bucket: the enclosing type's members.
    std::vector<wxString> m_globalSymbols;   ///< Cached bucket: top-level symbols (keyed by hash).
    std::vector<wxString> m_globalVariables; ///< Cached bucket: module-level variables (keyed by hash).
    std::size_t m_globalCompletionsHash = 0; ///< `SymbolTable` hash the global buckets were built for.
    bool m_globalCompletionsReady = false;   ///< Whether the global buckets have been built.
    /// Accumulated insert span awaiting a coalesced transformer pass.
    /// `m_pendingInsertStart < 0` means nothing pending. A burst of
    /// `EVT_STC_MODIFIED` inserts (multi-line indent, paste) folds into a
    /// single deferred `onTextInserted` instead of one call per event.
    int m_pendingInsertStart = -1;
    int m_pendingInsertEnd = -1;
    /// Per-line "did this change since the last save?" tracker — drives
    /// the change-margin markers. Snapshot is taken on file load and on
    /// every `SAVEPOINTREACHED` (save or undo back to clean).
    LineHistory m_lineHistory;
    /// True when the `editor.changeTracking` config switch is on. When
    /// off the margin is hidden and every change-tracker handler short
    /// circuits so no per-modify or per-save work happens.
    bool m_changeTracking = true;
    /// Restart on each text-changing modify event; on fire submits a
    /// snapshot to DocumentManager::submitIntellisense.
    wxTimer m_intellisenseTimer;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
