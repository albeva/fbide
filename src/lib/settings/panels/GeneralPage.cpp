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
    hbox(tr("dialogs.settings.general.editorSettings"), { .border = 0 }, [&] {
        vbox({ .proportion = 1 }, [&] {
            checkBox(m_autoIndent,      tr("dialogs.settings.general.autoIndent"));
            checkBox(m_indentGuide,     tr("dialogs.settings.general.indentGuides"));
            checkBox(m_showWhiteSpaces, tr("dialogs.settings.general.whitespace"));
            checkBox(m_showLineEndings, tr("dialogs.settings.general.lineEndings"));
            checkBox(m_braceHighlight,  tr("dialogs.settings.general.braceHighlight"));
            spinCtrl(m_edgeColumn, tr("dialogs.settings.general.rightMarginWidth"), 1, 200, {});
        });

        separator({ .space = false });

        vbox({ .proportion = 1 }, [&] {
            checkBox(m_syntaxHighlight, tr("dialogs.settings.general.syntaxHighlight"));
            checkBox(m_showLineNumbers, tr("dialogs.settings.general.lineNumbers"));
            checkBox(m_showRightMargin, tr("dialogs.settings.general.rightMargin"));
            checkBox(m_foldMargin,      tr("dialogs.settings.general.foldMargin"));
            checkBox(m_splashScreen,    tr("dialogs.settings.general.splashScreen"));
            spinCtrl(m_tabSize, tr("dialogs.settings.general.tabSize"), 1, 16, {});
        });
    });

    // Language section
    vbox(tr("dialogs.settings.general.language"), {}, [&] {
        hbox({ .center = true, .border = 0 }, [&] {
            text(tr("dialogs.settings.general.languageSelect"), { .proportion = 1, .expand = false });
            choice(m_language, getContext().getConfig().getAllLanguages(), { .expand = false })->SetMinSize(wxSize(200, -1));
        });

        // Restart warning
        text(tr("dialogs.settings.general.languageRestart"));
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
