//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Editor.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
#include "lexilla/FBSciLexer.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
enum class Margins : int {
    LineNumbers = 0,
    Fold = 1
};
constexpr auto operator+(const Margins& rhs) -> int {
    return static_cast<int>(rhs);
}

auto isBrace(const int ch) -> bool {
    return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(Editor, wxStyledTextCtrl)
    EVT_STC_MARGINCLICK (wxID_ANY,       Editor::onMarginClick)
    EVT_STC_MODIFIED(wxID_ANY,           Editor::onModified)
    EVT_STC_UPDATEUI(wxID_ANY,           Editor::onUpdateUI)
    EVT_STC_ZOOM(wxID_ANY,               Editor::onZoom)
    EVT_SET_FOCUS(Editor::onFocus)
wxEND_EVENT_TABLE()
// clang-format on

Editor::Editor(wxWindow* parent, Context& ctx, const DocumentType type, const bool preview)
: wxStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
, m_ctx(ctx)
, m_docType(type)
, m_preview(preview) {
    applySettings();
}

void Editor::applySettings() {
    applyEditorSettings();
    defineFoldMargins();
    applyTheme();
    updateLineNumberMarginWidth();
    Refresh();
}

void Editor::applyEditorSettings() {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    const auto tabSize = editor.get_or("tabSize", 4);

    UsePopUp(wxSTC_POPUP_TEXT);
    SetTabWidth(tabSize);
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(tabSize);

    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    // EOL mode is applied per-document — see Document ctor + setEolMode.

    if (m_preview) {
        // Preview mode: hide all margins and decorations
        SetMarginWidth(+Margins::LineNumbers, 0);
        SetMarginWidth(+Margins::Fold, 0);
        // SetMarginWidth(2, 0);
        SetEdgeMode(wxSTC_EDGE_NONE);
        SetViewEOL(false);
        SetIndentationGuides(false);
        SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
        // Prevent horizontal scrollbar flashing on content changes
        SetScrollWidthTracking(false);
        SetScrollWidth(10000);
        return;
    }

    SetEdgeColumn(editor.get_or("edgeColumn", 80));
    SetViewEOL(editor.get_or("displayEOL", false));
    SetIndentationGuides(editor.get_or("indentGuide", false) ? wxSTC_IV_LOOKFORWARD : wxSTC_IV_NONE);
    SetEdgeMode(editor.get_or("longLine", false) ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(editor.get_or("whiteSpace", false) ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);

    // Line number margin
    SetMarginCount(2);
    SetMarginWidth(+Margins::LineNumbers, 0);
    SetMarginWidth(+Margins::Fold, 0);
}

void Editor::defineFoldMargins() {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    if (not editor.get_or("folderMargin", false)) {
        return;
    }

    SetMarginType(+Margins::Fold, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(+Margins::Fold, static_cast<int>(wxSTC_MASK_FOLDERS));
    SetMarginWidth(+Margins::Fold, 16);
    SetMarginSensitive(+Margins::Fold, true);

    const auto& theme = m_ctx.getTheme();
    const auto foldFg = theme.foreground(theme.getFoldMargin().foreground);
    const auto foldBg = theme.foreground(theme.getFoldMargin().background);

    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, foldBg, foldFg);
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, foldBg, foldFg);

    SetMarginBackground(+Margins::Fold, foldBg);
    SetFoldMarginColour(true, foldBg);
    SetFoldMarginHiColour(true, foldBg);
    SetProperty("fold", "1");
}

void Editor::applyTheme() {
    const auto& theme = m_ctx.getTheme();
    const auto& defaultEntry = theme.get(ThemeCategory::Default);
    const auto& defaultColors = defaultEntry.colors;

    m_font = wxFont(
        theme.getFontSize(),
        wxFONTFAMILY_MODERN,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL,
        false,
        theme.getFont()
    );

    StyleSetForeground(wxSTC_STYLE_DEFAULT, defaultColors.foreground);
    StyleSetBackground(wxSTC_STYLE_DEFAULT, defaultColors.background);
    StyleSetFont(wxSTC_STYLE_DEFAULT, m_font);
    StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, defaultColors.background);
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, theme.getSeparator());

    // Line numbers
    applyColors(wxSTC_STYLE_LINENUMBER, theme.getLineNumber(), theme);
    StyleSetFont(wxSTC_STYLE_LINENUMBER, m_font);

    // Caret — no dedicated field, use default foreground.
    SetCaretForeground(defaultColors.foreground);

    // Selection
    const auto& sel = theme.getSelection();
    SetSelForeground(true, theme.foreground(sel.foreground));
    SetSelBackground(true, theme.background(sel.background));

    // Brace matching
    applyStyle(wxSTC_STYLE_BRACELIGHT, theme.getBrace(), theme);
    applyStyle(wxSTC_STYLE_BRACEBAD, theme.getBadBrace(), theme);

    // separator lines
    SetEdgeColour(theme.foreground(theme.getSeparator()));

    if (m_ctx.getConfigManager().config().get_or("editor.syntaxHighlight", true)) {
        switch (m_docType) {
        case DocumentType::FreeBASIC:
            applyFreebasicTheme();
            break;
        case DocumentType::HTML:
            applyHtmlTheme();
            break;
        case DocumentType::Properties:
            applyPropertiesTheme();
            break;
        case DocumentType::Text:
            applyTextTheme();
            break;
        }
    }
}

void Editor::applyFreebasicTheme() {
    const auto& theme = m_ctx.getTheme();

    SetILexer(FBSciLexer::Create());

    // Apply keywords
    const auto& keywords = m_ctx.getConfigManager().keywords();
    for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[idx]);
        SetKeyWords(static_cast<int>(idx), keywords.get_or(wxString(key), "").Lower());
    }

    for (const auto cat : kThemeCategories) {
        applyStyle(+cat, theme.get(cat), theme);
    }
}

void Editor::applyStyle(const int stcId, const Theme::Entry& style, const Theme& theme) {
    applyColors(stcId, style.colors, theme);
    StyleSetFont(stcId, m_font);
    StyleSetBold(stcId, style.bold);
    StyleSetItalic(stcId, style.italic);
    StyleSetUnderline(stcId, style.underlined);
}

void Editor::applyColors(const int stcId, const Theme::Colors& colors, const Theme& theme) {
    StyleSetForeground(stcId, theme.foreground(colors.foreground));
    StyleSetBackground(stcId, theme.background(colors.background));
}

void Editor::applyHtmlTheme() {
    const auto& theme = m_ctx.getTheme();
    SetLexer(wxSTC_LEX_HTML);

    applyStyle(wxSTC_H_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_H_TAG, theme.get(ThemeCategory::Keyword1), theme);
    applyStyle(wxSTC_H_TAGUNKNOWN, theme.get(ThemeCategory::Keyword1), theme);
    applyStyle(wxSTC_H_ATTRIBUTE, theme.get(ThemeCategory::Keyword2), theme);
    applyStyle(wxSTC_H_ATTRIBUTEUNKNOWN, theme.get(ThemeCategory::Keyword2), theme);
    applyStyle(wxSTC_H_NUMBER, theme.get(ThemeCategory::Number), theme);
    applyStyle(wxSTC_H_SINGLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_H_DOUBLESTRING, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_H_OTHER, theme.get(ThemeCategory::Keyword4), theme);
    applyStyle(wxSTC_H_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_H_ENTITY, theme.get(ThemeCategory::Keyword3), theme);
}

void Editor::applyPropertiesTheme() {
    const auto& theme = m_ctx.getTheme();
    SetLexer(wxSTC_LEX_PROPERTIES);
    applyStyle(wxSTC_PROPS_DEFAULT, theme.get(ThemeCategory::Default), theme);
    applyStyle(wxSTC_PROPS_COMMENT, theme.get(ThemeCategory::Comment), theme);
    applyStyle(wxSTC_PROPS_SECTION, theme.get(ThemeCategory::Preprocessor), theme);
    applyStyle(wxSTC_PROPS_ASSIGNMENT, theme.get(ThemeCategory::Operator), theme);
    applyStyle(wxSTC_PROPS_DEFVAL, theme.get(ThemeCategory::String), theme);
    applyStyle(wxSTC_PROPS_KEY, theme.get(ThemeCategory::Keyword1), theme);
}

void Editor::applyTextTheme() {
    SetLexer(wxSTC_LEX_NULL);
}

void Editor::updateLineNumberMarginWidth() {
    if (m_ctx.getConfigManager().config().get_or("editor.lineNumbers", true)) {
        const auto lineNrWidth = TextWidth(wxSTC_STYLE_LINENUMBER, "00001");
        SetMarginWidth(+Margins::LineNumbers, lineNrWidth);
    } else {
        SetMarginWidth(+Margins::LineNumbers, 0);
    }
}

void Editor::setDocType(const DocumentType type) {
    m_docType = type;
    applySettings();
}

void Editor::selectLine() {
    const auto line = GetCurrentLine();
    SetSelection(PositionFromLine(line), PositionFromLine(line + 1));
}

auto Editor::getWordAtCursor() -> wxString {
    if (const auto sel = GetSelectedText(); !sel.empty()) {
        return sel;
    }
    const auto pos = GetCurrentPos();
    const auto start = WordStartPosition(pos, true);
    const auto end = WordEndPosition(pos, true);
    if (start < end) {
        return GetTextRange(start, end);
    }
    return {};
}

auto Editor::findNext(const wxString& text, const int flags, const bool forward) -> bool {
    if (text.empty()) {
        return false;
    }

    // Convert wxFindReplaceDialog flags to wxSTC search flags
    int stcFlags = 0;
    if (flags & wxFR_WHOLEWORD) {
        stcFlags |= wxSTC_FIND_WHOLEWORD;
    }
    if (flags & wxFR_MATCHCASE) {
        stcFlags |= wxSTC_FIND_MATCHCASE;
    }

    if (forward) {
        SetTargetStart(GetCurrentPos());
        SetTargetEnd(GetLength());
    } else {
        // Search backward: from before current selection to start of document
        SetTargetStart(GetCurrentPos() - 1);
        SetTargetEnd(0);
    }
    SetSearchFlags(stcFlags);

    if (SearchInTarget(text) != -1) {
        SetSelection(GetTargetStart(), GetTargetEnd());
        EnsureCaretVisible();
        return true;
    }

    // Wrap around
    if (forward) {
        SetTargetStart(0);
        SetTargetEnd(GetCurrentPos());
    } else {
        SetTargetStart(GetLength());
        SetTargetEnd(GetCurrentPos());
    }
    SetSearchFlags(stcFlags);

    if (SearchInTarget(text) != -1) {
        SetSelection(GetTargetStart(), GetTargetEnd());
        EnsureCaretVisible();
        return true;
    }

    return false;
}

auto Editor::replaceNext(const wxString& findText, const wxString& replaceText, const int flags) -> bool {
    if (findText.empty()) {
        return false;
    }

    // Check if current selection matches the find text
    const auto selected = GetSelectedText();
    bool matches = false;
    if (flags & wxFR_MATCHCASE) {
        matches = (selected == findText);
    } else {
        matches = (selected.Lower() == findText.Lower());
    }

    if (matches) {
        ReplaceSelection(replaceText);
    }

    return findNext(findText, flags, true);
}

auto Editor::replaceAll(const wxString& findText, const wxString& replaceText, const int flags) -> int {
    if (findText.empty()) {
        return 0;
    }

    int stcFlags = 0;
    if (flags & wxFR_WHOLEWORD) {
        stcFlags |= wxSTC_FIND_WHOLEWORD;
    }
    if (flags & wxFR_MATCHCASE) {
        stcFlags |= wxSTC_FIND_MATCHCASE;
    }

    int count = 0;
    BeginUndoAction();

    SetTargetStart(0);
    SetTargetEnd(GetLength());
    SetSearchFlags(stcFlags);

    while (SearchInTarget(findText) != -1) {
        ReplaceTarget(replaceText);
        count++;
        SetTargetStart(GetTargetEnd());
        SetTargetEnd(GetLength());
    }

    EndUndoAction();
    return count;
}

void Editor::gotoLine(const wxString& input) {
    if (input.empty()) {
        return;
    }

    const auto colonPos = input.Find(':');
    const auto linePart = colonPos != wxNOT_FOUND ? input.Left(static_cast<size_t>(colonPos)) : input;
    const auto colPart = colonPos != wxNOT_FOUND ? input.Mid(static_cast<size_t>(colonPos) + 1) : wxString {};

    // Parse line — "e" means last line, otherwise 1-based number
    int line = 0;
    if (linePart.Lower() == "e") {
        line = GetLineCount() - 1;
    } else {
        long val = 0;
        if (!linePart.ToLong(&val) || val < 1) {
            return;
        }
        line = static_cast<int>(val) - 1;
    }

    GotoLine(line);

    // Parse column if present
    if (!colPart.empty()) {
        int col = 0;
        if (colPart.Lower() == "e") {
            col = GetLineEndPosition(line) - PositionFromLine(line);
        } else {
            long val = 0;
            if (colPart.ToLong(&val) && val >= 1) {
                col = static_cast<int>(val) - 1;
            }
        }
        GotoPos(PositionFromLine(line) + col);
    }

    EnsureCaretVisible();
}

void Editor::navigateToLine(const int line) {
    const int target = line - 1;
    if (GetCurrentLine() != target) {
        ScrollToLine(target - LinesOnScreen() / 2);
        GotoLine(target);
    }
    SetFocus();
    EnsureCaretVisible();
}

void Editor::commentSelection() {
    const auto lineStart = LineFromPosition(GetSelectionStart());
    const auto lineEnd = LineFromPosition(GetSelectionEnd());

    BeginUndoAction();
    for (auto line = lineStart; line <= lineEnd; line++) {
        InsertText(PositionFromLine(line), "'");
    }
    EndUndoAction();

    SetSelection(PositionFromLine(lineStart), GetLineEndPosition(lineEnd));
}

void Editor::uncommentSelection() {
    const auto lineStart = LineFromPosition(GetSelectionStart());
    const auto lineEnd = LineFromPosition(GetSelectionEnd());

    BeginUndoAction();
    for (auto line = lineStart; line <= lineEnd; line++) {
        const auto indent = GetLineIndentation(line);
        const auto pos = PositionFromLine(line) + indent;
        const auto text = GetLine(line).Trim(false).Lower();

        if (text.StartsWith("rem ") || text.StartsWith("rem\t")) {
            SetTargetStart(pos);
            SetTargetEnd(pos + 3);
            ReplaceTarget("");
        } else if (text.StartsWith("'")) {
            SetTargetStart(pos);
            SetTargetEnd(pos + 1);
            ReplaceTarget("");
        }
    }
    EndUndoAction();

    SetSelection(PositionFromLine(lineStart), GetLineEndPosition(lineEnd));
}

void Editor::onUpdateUI(wxStyledTextEvent& event) {
    event.Skip();
    updateStatusBar();
    updateBraceMatch();
}

void Editor::onZoom(wxStyledTextEvent&) {
    updateLineNumberMarginWidth();
}

void Editor::updateBraceMatch() {
    if (!m_ctx.getConfigManager().config().get_or("editor.braceHighlight", true)) {
        return;
    }

    const auto pos = GetCurrentPos();
    const auto ch = GetCharAt(pos);

    if (isBrace(ch)) {
        const auto match = BraceMatch(pos);
        if (match != wxSTC_INVALID_POSITION) {
            BraceHighlight(pos, match);
        } else {
            BraceBadLight(pos);
        }
    } else {
        BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
    }
}

void Editor::updateStatusBar() const {
    const auto pos = GetCurrentPos();
    const auto line = LineFromPosition(pos) + 1;
    const auto col = GetColumn(pos) + 1;
    auto* frame = m_ctx.getUIManager().getMainFrame();
    frame->SetStatusText(wxString::Format("%d : %d", line, col), 1);

    if (const auto* doc = m_ctx.getDocumentManager().findByEditor(this)) {
        frame->SetStatusText(wxString::FromUTF8(doc->getEolMode().toString()), 2);
        frame->SetStatusText(wxString::FromUTF8(doc->getEncoding().toString()), 3);
    } else {
        frame->SetStatusText("", 2);
        frame->SetStatusText("", 3);
    }
}

void Editor::onFocus(wxFocusEvent& event) {
    event.Skip();
    updateStatusBar();
    const auto state = m_docType == DocumentType::FreeBASIC
                         ? UIState::FocusedValidSourceFile
                         : UIState::FocusedUnknownFile;
    m_ctx.getUIManager().setDocumentState(state);
}

void Editor::onMarginClick(wxStyledTextEvent& event) {
    if (event.GetMargin() == +Margins::Fold) {
        const auto lineClick = LineFromPosition(event.GetPosition());
        const auto levelClick = GetFoldLevel(lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold(lineClick);
        }
    }
}

void Editor::onModified(wxStyledTextEvent& event) {
    event.Skip();
    const auto mod = event.GetModificationType();
    if ((mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) == 0) {
        return;
    }
    m_ctx.getDocumentManager().updateActiveTabTitle();
}
