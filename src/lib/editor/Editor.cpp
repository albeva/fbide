//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Editor.hpp"
#include "DocumentManager.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeOld.hpp"
#include "lexilla/FBSciLexer.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(Editor, wxStyledTextCtrl)
    EVT_STC_MODIFIED(wxID_ANY,  Editor::onModified)
    EVT_STC_UPDATEUI(wxID_ANY,  Editor::onUpdateUI)
    EVT_STC_ZOOM(wxID_ANY,      Editor::onZoom)
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
    applyTheme();
    updateLineNumberMarginWidth();
}

void Editor::applyEditorSettings() {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    const auto tabSize = editor.get_or("tabSize", 4);

    SetTabWidth(tabSize);
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(tabSize);

    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    SetEOLMode(wxSTC_EOL_LF);

    if (m_preview) {
        // Preview mode: hide all margins and decorations
        SetMarginWidth(0, 0);
        SetMarginWidth(1, 0);
        SetMarginWidth(2, 0);
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
    SetIndentationGuides(editor.get_or("indentGuide", false));
    SetEdgeMode(editor.get_or("longLine", false) ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(editor.get_or("whiteSpace", false) ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);

    // Line number margin
    SetMarginWidth(1, 0);

    // Fold margin
    if (editor.get_or("folderMargin", false)) {
        SetMarginType(2, wxSTC_MARGIN_SYMBOL);
        SetMarginMask(2, static_cast<int>(wxSTC_MASK_FOLDERS));
        SetMarginWidth(2, 14);
        SetMarginSensitive(2, true);

        SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
        MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, "white", "gray");
        MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, "white", "gray");

        SetProperty("fold", "1");
        SetProperty("fold.comment", "1");
        SetProperty("fold.compact", "1");
        SetProperty("fold.preprocessor", "1");
    } else {
        SetMarginWidth(2, 0);
        SetMarginSensitive(2, false);
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

void Editor::applyTheme() {
    const auto& theme = m_ctx.getTheme();
    const auto& editor = theme.getDefault();

    // Default style
    const auto defaultFont = wxFont(
        editor.fontSize,
        wxFONTFAMILY_MODERN,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL,
        false,
        editor.fontName
    );

    StyleSetForeground(wxSTC_STYLE_DEFAULT, editor.foreground);
    StyleSetBackground(wxSTC_STYLE_DEFAULT, editor.background);
    StyleSetFont(wxSTC_STYLE_DEFAULT, defaultFont);

    // Propagate default style to all styles before applying overrides
    StyleClearAll();

    // Line numbers
    const auto& lineNum = theme.getLineNumber();
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, lineNum.foreground);
    StyleSetBackground(wxSTC_STYLE_LINENUMBER, lineNum.background);
    StyleSetFont(wxSTC_STYLE_LINENUMBER, defaultFont);

    // Caret
    SetCaretForeground(editor.caretColour);

    // Selection
    const auto& sel = theme.getSelection();
    SetSelBackground(true, sel.background);
    SetSelForeground(true, sel.foreground);

    // Brace matching
    const auto& brace = theme.getBrace();
    StyleSetForeground(wxSTC_STYLE_BRACELIGHT, brace.foreground);
    StyleSetBackground(wxSTC_STYLE_BRACELIGHT, brace.background);
    StyleSetBold(wxSTC_STYLE_BRACELIGHT, brace.fontStyle.bold);

    const auto& badBrace = theme.getBadBrace();
    StyleSetForeground(wxSTC_STYLE_BRACEBAD, badBrace.foreground);
    StyleSetBackground(wxSTC_STYLE_BRACEBAD, badBrace.background);
    StyleSetBold(wxSTC_STYLE_BRACEBAD, badBrace.fontStyle.bold);

    if (m_ctx.getConfigManager().config().get_or("editor.syntaxHighlight", true)) {
        switch (m_docType) {
        case DocumentType::FreeBASIC:
            applyFreebasicTheme();
            break;
        case DocumentType::HTML:
            applyHtmlTheme();
            break;
        case DocumentType::Text:
            applyTextTheme();
            break;
        }
    }
}

void Editor::applyFreebasicTheme() {
    using enum ThemeCategory;

    const auto& theme = m_ctx.getTheme();
    const auto& editor = theme.getDefault();

    SetILexer(FBSciLexer::Create());

    // Apply keywords
    // TODO: add keyword group 5
    const auto& groups = m_ctx.getConfigManager().keywords().at("groups");
    for (std::size_t grp = 0; grp < 4; grp++) {
        wxString key;
        key.Printf("group%zu", grp + 1);
        SetKeyWords(static_cast<int>(grp), groups.get_or(key, ""));
    }

    // applyStyle(+Default, theme.getStyle(Theme::Default), editor); // TODO: define proper default
    applyStyle(+Comment, theme.getStyle(ThemeOld::Comment), editor);
    applyStyle(+MultilineComment, theme.getStyle(ThemeOld::Comment), editor);
    applyStyle(+Number, theme.getStyle(ThemeOld::Number), editor);
    applyStyle(+String, theme.getStyle(ThemeOld::String), editor);
    applyStyle(+StringOpen, theme.getStyle(ThemeOld::StringEol), editor);
    applyStyle(+Identifier, theme.getStyle(ThemeOld::Identifier), editor);
    applyStyle(+Keyword1, theme.getStyle(ThemeOld::Keyword), editor);
    applyStyle(+Keyword2, theme.getStyle(ThemeOld::Keyword2), editor);
    applyStyle(+Keyword3, theme.getStyle(ThemeOld::Keyword3), editor);
    applyStyle(+Keyword4, theme.getStyle(ThemeOld::Keyword4), editor);
    applyStyle(+Keyword5, theme.getStyle(ThemeOld::Keyword), editor); // TODO: keyword 5
    applyStyle(+Operator, theme.getStyle(ThemeOld::Operator), editor);
    applyStyle(+Label, theme.getStyle(ThemeOld::Identifier), editor);    // TODO: label
    applyStyle(+Constant, theme.getStyle(ThemeOld::Identifier), editor); // TODO: constant
    applyStyle(+Preprocessor, theme.getStyle(ThemeOld::Preprocessor), editor);
    applyStyle(+Error, theme.getStyle(ThemeOld::Identifier), editor);
}

void Editor::applyStyle(const int stcId, const ThemeOld::ItemStyle& style, const ThemeOld::EditorStyle& editor) {
    StyleSetForeground(stcId, style.foreground);
    StyleSetBackground(stcId, style.background);

    const auto font = wxFont(
        style.fontSize > 0 ? style.fontSize : editor.fontSize,
        wxFONTFAMILY_MODERN,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL,
        false,
        style.fontName.empty() ? editor.fontName : style.fontName
    );
    StyleSetFont(stcId, font);

    StyleSetBold(stcId, style.fontStyle.bold);
    StyleSetItalic(stcId, style.fontStyle.italic);
    StyleSetUnderline(stcId, style.fontStyle.underline);
    StyleSetVisible(stcId, !style.fontStyle.hidden);
    StyleSetCase(stcId, style.letterCase);
}

void Editor::applyHtmlTheme() {
    const auto& theme = m_ctx.getTheme();
    const auto& editor = theme.getDefault();
    SetLexer(wxSTC_LEX_HTML);

    applyStyle(wxSTC_H_TAG, theme.getStyle(ThemeOld::ItemKind::Keyword), editor);
    applyStyle(wxSTC_H_TAGUNKNOWN, theme.getStyle(ThemeOld::ItemKind::Keyword), editor);
    applyStyle(wxSTC_H_ATTRIBUTE, theme.getStyle(ThemeOld::ItemKind::Keyword2), editor);
    applyStyle(wxSTC_H_ATTRIBUTEUNKNOWN, theme.getStyle(ThemeOld::ItemKind::Keyword2), editor);
    applyStyle(wxSTC_H_NUMBER, theme.getStyle(ThemeOld::ItemKind::Number), editor);
    applyStyle(wxSTC_H_SINGLESTRING, theme.getStyle(ThemeOld::ItemKind::String), editor);
    applyStyle(wxSTC_H_DOUBLESTRING, theme.getStyle(ThemeOld::ItemKind::StringEol), editor);
    applyStyle(wxSTC_H_COMMENT, theme.getStyle(ThemeOld::ItemKind::Comment), editor);
    applyStyle(wxSTC_H_ENTITY, theme.getStyle(ThemeOld::ItemKind::Keyword3), editor);
    applyStyle(wxSTC_H_OTHER, theme.getStyle(ThemeOld::ItemKind::Keyword4), editor);
}

void Editor::applyTextTheme() {
    SetLexer(wxSTC_LEX_NULL);
}

void Editor::updateLineNumberMarginWidth() {
    if (m_ctx.getConfigManager().config().get_or("editor.lineNumbers", true)) {
        const auto lineNrWidth = TextWidth(wxSTC_STYLE_LINENUMBER, "00001");
        SetMarginWidth(0, lineNrWidth);
    } else {
        SetMarginWidth(0, 0);
    }
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

namespace {
auto isBrace(const int ch) -> bool {
    return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}
} // namespace

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
    m_ctx.getUIManager().getMainFrame()->SetStatusText(wxString::Format("%d : %d", line, col), 1);
}

void Editor::onFocus(wxFocusEvent& event) {
    event.Skip();
    updateStatusBar();
    const auto state = m_docType == DocumentType::FreeBASIC
                         ? UIState::FocusedValidSourceFile
                         : UIState::FocusedUnknownFile;
    m_ctx.getUIManager().setDocumentState(state);
}

void Editor::onModified(wxStyledTextEvent& event) {
    event.Skip();
    const auto mod = event.GetModificationType();
    if ((mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) == 0) {
        return;
    }
    m_ctx.getDocumentManager().updateActiveTabTitle();
}
