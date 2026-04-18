//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerPage.hpp"
#include "help/HelpManager.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/Lang.hpp"
using namespace fbide;

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    auto& cfg = getContext().getConfigManager();
    m_compilerPath   = cfg.config_or("compiler.path",           "");
    m_compileCommand = cfg.config_or("compiler.compileCommand", "");
    m_runCommand     = cfg.config_or("compiler.runCommand",     "");
#ifdef __WXMSW__
    m_helpFile = cfg.config_or("paths.helpFile", std::string {});
#endif
}

void CompilerPage::create() {
    // makeTitle(LangId::SettingsCompilerAndPaths);
    vbox(getContext().getLang()[LangId::SettingsCompilerAndPaths], { .border = 0 }, [&] {
        compilerPath();
        spacer();
        compilerCommand();
        spacer();
        runCommand();
#ifdef __WXMSW__
        spacer();
        helpFile();
#endif
    });
}

void CompilerPage::apply() {
    auto& cfg = getContext().getConfigManager().getConfig();
    auto& compiler = cfg["compiler"];
    compiler["compileCommand"] = m_compileCommand.ToStdString();
    compiler["runCommand"]     = m_runCommand.ToStdString();
#ifdef __WXMSW__
    cfg["paths"]["helpFile"] = m_helpFile.ToStdString();
#endif
    const wxString existing = toml::find_or(compiler, "path", std::string {});
    if (m_compilerPath != existing) {
        compiler["path"] = m_compilerPath.ToStdString();
        getContext().getCompilerManager().resetFbcVersion();
    }
}

void CompilerPage::compilerPath() {
    const auto [tf, btn] = makeFileEntry(m_compilerPath, LangId::SettingsCompilerPath);
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select compiler", "", "",
#ifdef __WXMSW__
            "FreeBASIC (fbc.exe)|fbc.exe|All programs (*.exe)|*.exe",
#else
            "All files (*)|*",
#endif
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_compilerPath = getContext().getConfigManager().relative(dlg.GetPath());
        }
        tf->SetValue(m_compilerPath);
    });
}

void CompilerPage::compilerCommand() {
    makeEntryField(m_compileCommand, LangId::SettingsCompilerCommand);
}

void CompilerPage::runCommand() {
    makeEntryField(m_runCommand, LangId::SettingsRunCmd);
}

#ifdef __WXMSW__
void CompilerPage::helpFile() {
    const auto [tf, btn] = makeFileEntry(m_helpFile, LangId::SettingsHelpFile);
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select help file", "", "",
            getContext().getLang()[LangId::SettingsHelpFileFilter] + "|*.chm",
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_helpFile = getContext().getConfigManager().relative(dlg.GetPath());
            HelpManager::verifyHelpFileAccessible(this, m_helpFile);
        }
        tf->SetValue(m_helpFile);
    });
}
#endif

auto CompilerPage::makeEntryField(wxString& value, const LangId lang) -> Unowned<wxTextCtrl> {
    const auto lbl = text(lang, {});
    const auto tf = textField(value, {});
    connect(lbl, tf);
    return tf;
}

auto CompilerPage::makeFileEntry(wxString& value, const LangId lang) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>> {
    const auto lbl = text(lang, {});
    Unowned<wxButton> btn;
    Unowned<wxTextCtrl> tf;
    hbox({ .center = true, .border = 0 }, [&] {
        tf = textField(value, { .proportion = 1 });
        connect(lbl, tf);
        btn = button("...", {});
    });
    return std::make_pair(tf, btn);
}
