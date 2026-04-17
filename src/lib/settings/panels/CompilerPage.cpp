//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerPage.hpp"
#include "lib/app/Context.hpp"
#include "../../help/HelpManager.hpp"
#include "lib/compiler/CompilerManager.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_compilerPath(getConfig().getCompilerPath())
, m_compileCommand(getConfig().getCompileCommand())
, m_runCommand(getConfig().getRunCommand())
#ifdef __WXMSW__
, m_helpFile(getConfig().getHelpFile())
#endif
{}

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
    auto& config = getConfig();
    config.setCompileCommand(m_compileCommand);
    config.setRunCommand(m_runCommand);
    config.setHelpFile(m_helpFile);
    if (m_compilerPath != config.getCompilerPath()) {
        config.setCompilerPath(m_compilerPath);
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
            m_compilerPath = dlg.GetPath();
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
            m_helpFile = dlg.GetPath();
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
