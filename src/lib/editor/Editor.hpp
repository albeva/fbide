//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "DocumentType.hpp"
#include "config/ThemeOld.hpp"

namespace fbide {
class Context;

/// Scintilla-based code editor.
class Editor final : public wxStyledTextCtrl {
public:
    NO_COPY_AND_MOVE(Editor)

    /// Create editor as child of parent window for given document type.
    /// If preview is true, hides all margins and decorations.
    Editor(wxWindow* parent, Context& ctx, DocumentType type = DocumentType::FreeBASIC, bool preview = false);

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

private:
    void onModified(wxStyledTextEvent& event);
    void onUpdateUI(wxStyledTextEvent& event);
    void onZoom(wxStyledTextEvent& event);
    void onFocus(wxFocusEvent& event);
    void updateBraceMatch();
    void applyEditorSettings();
    void applyTheme();
    void applyStyle(int stcId, const ThemeOld::ItemStyle& style, const ThemeOld::EditorStyle& editor);
    void applyFreebasicTheme();
    void applyHtmlTheme();
    void applyTextTheme();
    void updateLineNumberMarginWidth();

    Context& m_ctx;
    DocumentType m_docType;
    bool m_preview;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
