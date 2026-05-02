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
    void onMarginClick(wxStyledTextEvent& event);
    void onModified(wxStyledTextEvent& event);
    void onUpdateUI(wxStyledTextEvent& event);
    void postUpdateUI();
    void onZoom(wxStyledTextEvent& event);
    void onCharAdded(wxStyledTextEvent& event);
    void onFocus(wxFocusEvent& event);
    void onIntellisenseTimer(wxTimerEvent& event);
    void onHotSpotClick(wxStyledTextEvent& event);
    void onKeyDown(wxKeyEvent& event);
    void onKeyUp(wxKeyEvent& event);
    void onKillFocus(wxFocusEvent& event);
    void setIncludeHotspots(bool active);
    void updateBraceMatch();
    void applyEditorSettings();
    void defineFoldMargins();
    void applyTheme();
    void applyStyle(int stcId, const Theme::Entry& style, const Theme& theme);
    void applyColors(int stcId, const Theme::Colors& colors, const Theme& theme);
    void applyFreebasicTheme();
    void applyHtmlTheme();
    void applyPropertiesTheme();
    void applyTextTheme();
    void updateLineNumberMarginWidth();

    Context& m_ctx;
    CodeTransformer* m_transformer;
    DocumentType m_docType;
    wxFont m_font;
    bool m_preview;
    bool m_insertHandled = false;
    bool m_editorLocked = false;
    bool m_includeHotspotsActive = false;
    int m_lastCaretPos = 0;
    bool m_callPostUpdate = false;
    /// Restart on each text-changing modify event; on fire submits a
    /// snapshot to DocumentManager::submitIntellisense.
    wxTimer m_intellisenseTimer;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
