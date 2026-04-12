//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Editor.hpp"
#include "DocumentManager.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Theme.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(Editor, wxStyledTextCtrl)
    EVT_STC_MODIFIED(wxID_ANY, Editor::onModified)
    EVT_STC_UPDATEUI(wxID_ANY, Editor::onUpdateUI)
    EVT_SET_FOCUS(Editor::onFocus)
wxEND_EVENT_TABLE()
// clang-format on

Editor::Editor(wxWindow* parent, Context& ctx, const DocumentType type)
: wxStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
, m_ctx(ctx)
, m_docType(type) {
    applySettings();
}

void Editor::applySettings() {
    applyEditorSettings();
    applyTheme();
}

void Editor::applyEditorSettings() {
    const auto& config = m_ctx.getConfig();

    SetTabWidth(config.getTabSize());
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(config.getTabSize());

    SetEdgeColumn(config.getEdgeColumn());
    SetScrollWidth(1);
    SetScrollWidthTracking(true);
    SetEOLMode(wxSTC_EOL_LF);
    SetViewEOL(config.getDisplayEOL());
    SetIndentationGuides(config.getIndentGuide());
    SetEdgeMode(config.getLongLine() ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(config.getWhiteSpace() ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);

    // Line number margin
    if (config.getLineNumbers()) {
        const auto lineNrWidth = TextWidth(wxSTC_STYLE_LINENUMBER, "00001");
        SetMarginWidth(0, lineNrWidth);
    } else {
        SetMarginWidth(0, 0);
    }
    SetMarginWidth(1, 0);

    // Fold margin
    if (config.getFolderMargin()) {
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

    if (m_ctx.getConfig().getSyntaxHighlight()) {
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
    const auto& theme = m_ctx.getTheme();
    const auto& editor = theme.getDefault();

    SetLexer(wxSTC_LEX_FREEBASIC);

    // Apply keywords
    const auto& keywords = m_ctx.getKeywords();
    for (std::size_t grp = 0; grp < Keywords::GROUP_COUNT; grp++) {
        SetKeyWords(static_cast<int>(grp), keywords.getGroup(grp));
    }

    // VB style mappings
    constexpr std::array styles {
        wxSTC_B_DEFAULT,
        wxSTC_B_COMMENT,
        wxSTC_B_NUMBER,
        wxSTC_B_KEYWORD,
        wxSTC_B_STRING,
        wxSTC_B_PREPROCESSOR,
        wxSTC_B_OPERATOR,
        wxSTC_B_IDENTIFIER,
        wxSTC_B_DATE,
        wxSTC_B_STRINGEOL,
        wxSTC_B_KEYWORD2,
        wxSTC_B_KEYWORD3,
        wxSTC_B_KEYWORD4,
        wxSTC_B_CONSTANT,
        wxSTC_B_ASM
    };

    for (size_t idx = 1; idx < styles.size(); idx++) {
        const auto& style = theme.getStyle(static_cast<Theme::ItemKind>(idx));
        const auto stcId = styles[idx];
        applyStyle(stcId, style, editor);
    }

    applyStyle(wxSTC_B_COMMENTBLOCK, theme.getStyle(Theme::ItemKind::Comment), editor);
    applyStyle(wxSTC_B_DOCBLOCK, theme.getStyle(Theme::ItemKind::Comment), editor);
    applyStyle(wxSTC_B_DOCLINE, theme.getStyle(Theme::ItemKind::Comment), editor);
    applyStyle(wxSTC_B_DOCKEYWORD, theme.getStyle(Theme::ItemKind::Comment), editor);
    applyStyle(wxSTC_B_HEXNUMBER, theme.getStyle(Theme::ItemKind::Number), editor);
    applyStyle(wxSTC_B_BINNUMBER, theme.getStyle(Theme::ItemKind::Number), editor);
}

void Editor::applyStyle(const int stcId, const Theme::ItemStyle& style, const Theme::EditorStyle& editor) {
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
    SetLexer(wxSTC_LEX_HTML);
}

void Editor::applyTextTheme() {
    SetLexer(wxSTC_LEX_NULL);
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

void Editor::updateBraceMatch() {
    if (!m_ctx.getConfig().getBraceHighlight()) {
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
}

void Editor::onModified(wxStyledTextEvent& event) {
    event.Skip();
    const auto mod = event.GetModificationType();
    if ((mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) == 0) {
        return;
    }
    m_ctx.getDocumentManager().updateActiveTabTitle();
}
