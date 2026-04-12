//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "GeneralPage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

GeneralPage::GeneralPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_autoIndent(getConfig().getAutoIndent())
, m_indentGuide(getConfig().getIndentGuide())
, m_showWhiteSpaces(getConfig().getWhiteSpace())
, m_showLineEndings(getConfig().getDisplayEOL())
, m_braceHighlight(getConfig().getBraceHighlight())
, m_syntaxHighlight(getConfig().getSyntaxHighlight())
, m_showLineNumbers(getConfig().getLineNumbers())
, m_showRightMargin(getConfig().getLongLine())
, m_foldMargin(getConfig().getFolderMargin())
, m_splashScreen(getConfig().getSplashScreen())
, m_edgeColumn(getConfig().getEdgeColumn())
, m_tabSize(getConfig().getTabSize())
, m_language(getConfig().getLanguage()) {}

void GeneralPage::layout() {
    makeTitle(LangId::SettingsEditorSettings);

    // Editor settings: left and right columns
    const auto editorHBox = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    getVBox()->Add(editorHBox, 0, wxGROW, 5);

    const auto leftVBox = make_unowned<wxBoxSizer>(wxVERTICAL);
    editorHBox->Add(leftVBox, 1, wxALIGN_TOP | wxALL, 5);

    makeSeparator(editorHBox, wxVERTICAL);

    const auto rightVBox = make_unowned<wxBoxSizer>(wxVERTICAL);
    editorHBox->Add(rightVBox, 1, wxALIGN_TOP | wxALL, 5);

    // Left column
    makeCheckBox(leftVBox, m_autoIndent, LangId::SettingsAutoIndent);
    makeCheckBox(leftVBox, m_indentGuide, LangId::SettingsIndentGuides);
    makeCheckBox(leftVBox, m_showWhiteSpaces, LangId::SettingsWhitespace);
    makeCheckBox(leftVBox, m_showLineEndings, LangId::SettingsLineEndings);
    makeCheckBox(leftVBox, m_braceHighlight, LangId::SettingsBraceHighlight);
    makeSpinCtrl(leftVBox, m_edgeColumn, LangId::SettingsRightMarginWidth, 1, 200);

    // Right column
    makeCheckBox(rightVBox, m_syntaxHighlight, LangId::SettingsSyntaxHighlight);
    makeCheckBox(rightVBox, m_showLineNumbers, LangId::SettingsLineNumbers);
    makeCheckBox(rightVBox, m_showRightMargin, LangId::SettingsRightMargin);
    makeCheckBox(rightVBox, m_foldMargin, LangId::SettingsFoldMargin);
    makeCheckBox(rightVBox, m_splashScreen, LangId::SettingsSplashScreen);
    makeSpinCtrl(rightVBox, m_tabSize, LangId::SettingsTabSize, 1, 16);

    // Language section
    makeTitle(LangId::SettingsLanguage);

    // Scan for language files
    const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    getVBox()->Add(row, 0, wxEXPAND | wxALL, 5);
    makeText(row, LangId::SettingsLanguageSelect, wxALIGN_CENTER_VERTICAL);
    row->AddSpacer(5);
    makeChoice(row, m_language, getContext().getConfig().getAllLanguages());

    // Restart warning
    makeText(getVBox(), LangId::SettingsLanguageRestart);
}

void GeneralPage::apply() {
    auto& config = getConfig();
    config.setAutoIndent(m_autoIndent);
    config.setIndentGuide(m_indentGuide);
    config.setWhiteSpace(m_showWhiteSpaces);
    config.setDisplayEOL(m_showLineEndings);
    config.setBraceHighlight(m_braceHighlight);
    config.setSyntaxHighlight(m_syntaxHighlight);
    config.setLineNumbers(m_showLineNumbers);
    config.setLongLine(m_showRightMargin);
    config.setFolderMargin(m_foldMargin);
    config.setSplashScreen(m_splashScreen);
    config.setEdgeColumn(m_edgeColumn);
    config.setTabSize(m_tabSize);
    config.setLanguage(m_language);
}
