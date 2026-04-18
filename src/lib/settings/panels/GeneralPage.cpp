//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "GeneralPage.hpp"
#include "app/Context.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
#include "config/Lang.hpp"
using namespace fbide;

GeneralPage::GeneralPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    auto& cfg = getContext().getConfigManager();
    m_autoIndent      = cfg.config_or("editor.autoIndent",      true);
    m_indentGuide     = cfg.config_or("editor.indentGuide",     false);
    m_showWhiteSpaces = cfg.config_or("editor.whiteSpace",      false);
    m_showLineEndings = cfg.config_or("editor.displayEOL",      false);
    m_braceHighlight  = cfg.config_or("editor.braceHighlight",  true);
    m_syntaxHighlight = cfg.config_or("editor.syntaxHighlight", true);
    m_showLineNumbers = cfg.config_or("editor.lineNumbers",     true);
    m_showRightMargin = cfg.config_or("editor.longLine",        false);
    m_foldMargin      = cfg.config_or("editor.folderMargin",    false);
    m_splashScreen    = cfg.config_or("general.splashScreen",   true);
    m_edgeColumn      = cfg.config_or("editor.edgeColumn",      80);
    m_tabSize         = cfg.config_or("editor.tabSize",         4);
    m_language        = getConfig().getLanguage();
}

void GeneralPage::create() {
    const auto& lang = getContext().getLang();
    hbox(lang[LangId::SettingsEditorSettings], { .border = 0 }, [&] {
        vbox({ .proportion = 1 }, [&] {
            checkBox(m_autoIndent, LangId::SettingsAutoIndent);
            checkBox(m_indentGuide, LangId::SettingsIndentGuides);
            checkBox(m_showWhiteSpaces, LangId::SettingsWhitespace);
            checkBox(m_showLineEndings, LangId::SettingsLineEndings);
            checkBox(m_braceHighlight, LangId::SettingsBraceHighlight);
            spinCtrl(m_edgeColumn, LangId::SettingsRightMarginWidth, 1, 200, {});
        });

        separator({ .space = false });

        vbox({ .proportion = 1 }, [&] {
            checkBox(m_syntaxHighlight, LangId::SettingsSyntaxHighlight);
            checkBox(m_showLineNumbers, LangId::SettingsLineNumbers);
            checkBox(m_showRightMargin, LangId::SettingsRightMargin);
            checkBox(m_foldMargin, LangId::SettingsFoldMargin);
            checkBox(m_splashScreen, LangId::SettingsSplashScreen);
            spinCtrl(m_tabSize, LangId::SettingsTabSize, 1, 16, {});
        });
    });

    // Language section
    vbox(lang[LangId::SettingsLanguage], {}, [&] {
        hbox({ .center = true, .border = 0 }, [&] {
            text(LangId::SettingsLanguageSelect, { .proportion = 1, .expand = false });
            choice(m_language, getContext().getConfig().getAllLanguages(), { .expand = false })->SetMinSize(wxSize(200, -1));
        });

        // Restart warning
        text(LangId::SettingsLanguageRestart);
    });
}

void GeneralPage::apply() {
    auto& cfg = getContext().getConfigManager().getConfig();
    auto& editor = cfg["editor"];
    editor["autoIndent"]      = m_autoIndent;
    editor["indentGuide"]     = m_indentGuide;
    editor["whiteSpace"]      = m_showWhiteSpaces;
    editor["displayEOL"]      = m_showLineEndings;
    editor["braceHighlight"]  = m_braceHighlight;
    editor["syntaxHighlight"] = m_syntaxHighlight;
    editor["lineNumbers"]     = m_showLineNumbers;
    editor["longLine"]        = m_showRightMargin;
    editor["folderMargin"]    = m_foldMargin;
    editor["edgeColumn"]      = m_edgeColumn;
    editor["tabSize"]         = m_tabSize;
    cfg["general"]["splashScreen"] = m_splashScreen;
    // Language persists via old Config (locale deferred)
    getConfig().setLanguage(m_language);
}
