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
using namespace fbide;

Editor::Editor(wxWindow* parent, Context& ctx, DocumentType type)
: wxStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
, m_ctx(ctx)
, m_docType(type) {
    applySettings();
    Bind(wxEVT_STC_MODIFIED, &Editor::onModified, this);
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
    StyleSetBold(wxSTC_STYLE_BRACELIGHT, (static_cast<int>(brace.fontStyle) & static_cast<int>(Theme::FontStyle::Bold)) != 0);

    const auto& badBrace = theme.getBadBrace();
    StyleSetForeground(wxSTC_STYLE_BRACEBAD, badBrace.foreground);
    StyleSetBackground(wxSTC_STYLE_BRACEBAD, badBrace.background);
    StyleSetBold(wxSTC_STYLE_BRACEBAD, (static_cast<int>(badBrace.fontStyle) & static_cast<int>(Theme::FontStyle::Bold)) != 0);

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
    for (int grp = 0; grp < Keywords::GROUP_COUNT; grp++) {
        SetKeyWords(grp, keywords.getGroup(grp));
    }

    // VB style mappings
    constexpr std::array styles {
        wxSTC_B_DEFAULT, wxSTC_B_COMMENT,
        wxSTC_B_NUMBER, wxSTC_B_KEYWORD,
        wxSTC_B_STRING, wxSTC_B_PREPROCESSOR,
        wxSTC_B_OPERATOR, wxSTC_B_IDENTIFIER,
        wxSTC_B_DATE, wxSTC_B_STRINGEOL,
        wxSTC_B_KEYWORD2, wxSTC_B_KEYWORD3,
        wxSTC_B_KEYWORD4, wxSTC_B_CONSTANT,
        wxSTC_B_ASM
    };

    for (size_t idx = 1; idx < styles.size(); idx++) {
        const auto& style = theme.getStyle(static_cast<Theme::ItemKind>(idx));
        const auto stcId = styles[idx];

        StyleSetForeground(stcId, style.foreground);
        StyleSetBackground(stcId, style.background);

        auto font = wxFont(
            style.fontSize > 0 ? style.fontSize : editor.fontSize,
            wxFONTFAMILY_MODERN,
            wxFONTSTYLE_NORMAL,
            wxFONTWEIGHT_NORMAL,
            false,
            style.fontName.empty() ? editor.fontName : style.fontName
        );
        StyleSetFont(stcId, font);

        const auto fs = static_cast<int>(style.fontStyle);
        StyleSetBold(stcId, (fs & static_cast<int>(Theme::FontStyle::Bold)) != 0);
        StyleSetItalic(stcId, (fs & static_cast<int>(Theme::FontStyle::Italic)) != 0);
        StyleSetUnderline(stcId, (fs & static_cast<int>(Theme::FontStyle::Underline)) != 0);
        StyleSetVisible(stcId, (fs & static_cast<int>(Theme::FontStyle::Hidden)) == 0);
        StyleSetCase(stcId, style.letterCase);
    }
}

void Editor::applyHtmlTheme() {
    SetLexer(wxSTC_LEX_HTML);
}

void Editor::applyTextTheme() {
    SetLexer(wxSTC_LEX_NULL);
}

void Editor::onModified(wxStyledTextEvent& event) {
    event.Skip();
    auto mod = event.GetModificationType();
    if ((mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) == 0) {
        return;
    }
    m_ctx.getDocumentManager().updateActiveTabTitle();
}
