//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "GeneralPage.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
auto currentLocaleFileName(Value cfg) -> wxString {
    return wxFileName(cfg.get_or("locale", "")).GetFullName();
}
} // namespace

GeneralPage::GeneralPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    auto cfg = getContext().getConfigManager().config();
    const auto editor = cfg.at("editor");
    m_autoIndent      = editor.get_or("autoIndent",      true);
    m_indentGuide     = editor.get_or("indentGuide",     false);
    m_showWhiteSpaces = editor.get_or("whiteSpace",      false);
    m_showLineEndings = editor.get_or("displayEOL",      false);
    m_braceHighlight  = editor.get_or("braceHighlight",  true);
    m_syntaxHighlight = editor.get_or("syntaxHighlight", true);
    m_showLineNumbers = editor.get_or("lineNumbers",     true);
    m_showRightMargin = editor.get_or("longLine",        false);
    m_foldMargin      = editor.get_or("folderMargin",    false);
    m_edgeColumn      = editor.get_or("edgeColumn",      80);
    m_tabSize         = editor.get_or("tabSize",         4);
    m_splashScreen    = cfg.get_or("general.splashScreen", true);
    m_language        = currentLocaleFileName(cfg);
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
            wxArrayString names;
            for (const auto& path : getContext().getConfigManager().getAllLanguages()) {
                names.Add(wxFileName(path).GetFullName());
            }
            choice(m_language, names, { .expand = false })->SetMinSize(wxSize(200, -1));
        });
    });
}

void GeneralPage::apply() {
    auto& cfgManager = getContext().getConfigManager();
    auto cfg = cfgManager.config();
    auto editor = cfg["editor"];
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

    // Swap locale file if the user picked a different language.
    if (!m_language.empty() && m_language != currentLocaleFileName(cfg)) {
        cfgManager.setCategoryPath(ConfigManager::Category::Locale, "locales/" + m_language);
        getContext().getUIManager().refreshUi();
    }
}
