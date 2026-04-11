//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Settings dialog with tabs for General, Themes, Keywords, and Compiler.
/// Works on copies of Config/Theme/Keywords — applies only on OK.
class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, Context& ctx);
    void create();

private:
    auto createGeneralTab(wxNotebook* notebook) -> Unowned<wxPanel>;
    auto createThemesTab(wxNotebook* notebook) -> Unowned<wxPanel>;
    auto createKeywordsTab(wxNotebook* notebook) -> Unowned<wxPanel>;
    auto createCompilerTab(wxNotebook* notebook) -> Unowned<wxPanel>;

    void applyChanges();

    // Theme tab helpers
    void onThemeTypeSelected(wxCommandEvent& event);
    void onThemeChanged(wxCommandEvent& event);
    void onSaveTheme(wxCommandEvent& event);
    void onForegroundColor(wxCommandEvent& event);
    void onBackgroundColor(wxCommandEvent& event);
    void setTypeSelection(int sel);
    void storeTypeSelection(int sel);

    // Keywords tab
    void onKeywordGroupChanged(wxCommandEvent& event);

    // Compiler tab
    void onCompilerBrowse(wxCommandEvent& event);
    void onHelpBrowse(wxCommandEvent& event);

    Context& m_ctx;

    // General tab controls
    Unowned<wxCheckBox> m_chkAutoIndent;
    Unowned<wxCheckBox> m_chkIndentGuides;
    Unowned<wxCheckBox> m_chkWhiteSpace;
    Unowned<wxCheckBox> m_chkLineEndings;
    Unowned<wxCheckBox> m_chkBraceHighlight;
    Unowned<wxCheckBox> m_chkSyntaxHighlight;
    Unowned<wxCheckBox> m_chkLineNumbers;
    Unowned<wxCheckBox> m_chkRightMargin;
    Unowned<wxCheckBox> m_chkFoldMargin;
    Unowned<wxCheckBox> m_chkSplashScreen;
    Unowned<wxSpinCtrl> m_spinEdgeColumn;
    Unowned<wxSpinCtrl> m_spinTabSize;
    Unowned<wxChoice> m_chLanguage;

    // Theme tab controls
    Unowned<wxListBox> m_themeTypeList;
    Unowned<wxChoice> m_themeChoice;
    Unowned<wxButton> m_btnForeground;
    Unowned<wxButton> m_btnBackground;
    Unowned<wxChoice> m_chFont;
    Unowned<wxCheckBox> m_chkBold;
    Unowned<wxCheckBox> m_chkItalic;
    Unowned<wxCheckBox> m_chkUnderline;
    Unowned<wxSpinCtrl> m_spinFontSize;

    // Keywords tab controls
    Unowned<wxChoice> m_chKeywordGroup;
    Unowned<wxTextCtrl> m_textKeywords;

    // Compiler tab controls
    Unowned<wxTextCtrl> m_textCompilerPath;
    Unowned<wxTextCtrl> m_textCompileCommand;
    Unowned<wxTextCtrl> m_textRunCommand;
    Unowned<wxTextCtrl> m_textHelpFile;

    // Theme editing state
    int m_themeTypeOld = 0;
    int m_keywordGroupOld = 0;

    // Working copies of keyword groups
    std::array<wxString, 4> m_keywordGroups;
};

} // namespace fbide
