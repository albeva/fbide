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

    hbox({ .proportion = 0, .flag = wxGROW, .border = 0 }, [&] {
        vbox({ .proportion = 1, .flag = wxALIGN_TOP | wxALL }, [&] {
            checkBox(m_autoIndent, LangId::SettingsAutoIndent);
            checkBox(m_indentGuide, LangId::SettingsIndentGuides);
            checkBox(m_showWhiteSpaces, LangId::SettingsWhitespace);
            checkBox(m_showLineEndings, LangId::SettingsLineEndings);
            checkBox(m_braceHighlight, LangId::SettingsBraceHighlight);
            spinCtrl(m_edgeColumn, LangId::SettingsRightMarginWidth, 1, 200, { .border = 0 });
        });

        separator();

        vbox({ .proportion = 1, .flag = wxALIGN_TOP | wxALL }, [&] {
            checkBox(m_syntaxHighlight, LangId::SettingsSyntaxHighlight);
            checkBox(m_showLineNumbers, LangId::SettingsLineNumbers);
            checkBox(m_showRightMargin, LangId::SettingsRightMargin);
            checkBox(m_foldMargin, LangId::SettingsFoldMargin);
            checkBox(m_splashScreen, LangId::SettingsSplashScreen);
            spinCtrl(m_tabSize, LangId::SettingsTabSize, 1, 16, { .border = 0 });
        });
    });

    // Language section
    makeTitle(LangId::SettingsLanguage);

    // Scan for language files
    hbox({ .flag = wxEXPAND | wxALL }, [&] {
        text(LangId::SettingsLanguageSelect, { .flag = wxALIGN_CENTER_VERTICAL });
        spacer();
        choice(m_language, getContext().getConfig().getAllLanguages());
    });

    // Restart warning
    text(LangId::SettingsLanguageRestart);
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
