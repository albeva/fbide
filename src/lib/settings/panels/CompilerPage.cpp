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
using namespace fbide;

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& cfg = getContext().getConfigManager().config();
    const auto& compiler = cfg.at("compiler");
    m_compilerPath   = compiler.get_or("path",           "");
    m_compileCommand = compiler.get_or("compileCommand", "");
    m_runCommand     = compiler.get_or("runCommand",     "");
#ifdef __WXMSW__
    m_helpFile = cfg.get_or("paths.helpFile", "");
#endif
}

void CompilerPage::create() {
    vbox(tr("dialogs.settings.compiler.compilerAndPaths"), { .border = 0 }, [&] {
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
    SetSizerAndFit(currentSizer());
}

void CompilerPage::apply() {
    auto& cfg = getContext().getConfigManager().config();
    auto& compiler = cfg["compiler"];
    compiler["compileCommand"] = m_compileCommand;
    compiler["runCommand"]     = m_runCommand;
#ifdef __WXMSW__
    cfg["paths"]["helpFile"] = m_helpFile;
#endif
    const wxString existing = compiler.get_or("path", "");
    if (m_compilerPath != existing) {
        compiler["path"] = m_compilerPath;
        getContext().getCompilerManager().resetFbcVersion();
    }
}

void CompilerPage::compilerPath() {
    const auto [tf, btn] = makeFileEntry(m_compilerPath, tr("dialogs.settings.compiler.compilerPath"));
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select compiler", "", "",
            getContext().getConfigManager().filePatterns({ "compiler", "all" }),
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_compilerPath = getContext().getConfigManager().relative(dlg.GetPath());
        }
        tf->SetValue(m_compilerPath);
    });
}

void CompilerPage::compilerCommand() {
    makeEntryField(m_compileCommand, tr("dialogs.settings.compiler.compilerCommand"));
}

void CompilerPage::runCommand() {
    makeEntryField(m_runCommand, tr("dialogs.settings.compiler.runCommand"));
}

#ifdef __WXMSW__
void CompilerPage::helpFile() {
    const auto [tf, btn] = makeFileEntry(m_helpFile, tr("dialogs.settings.compiler.helpFile"));
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select help file", "", "",
            getContext().getConfigManager().filePattern("help"),
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

auto CompilerPage::makeEntryField(wxString& value, const wxString& labelText) -> Unowned<wxTextCtrl> {
    const auto lbl = text(labelText, {});
    const auto tf = textField(value, {});
    connect(lbl, tf);
    return tf;
}

auto CompilerPage::makeFileEntry(wxString& value, const wxString& labelText) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>> {
    const auto lbl = text(labelText, {});
    Unowned<wxButton> btn;
    Unowned<wxTextCtrl> tf;
    hbox({ .center = true, .border = 0 }, [&] {
        tf = textField(value, { .proportion = 1 });
        connect(lbl, tf);
        btn = button("...", {});
    });
    return std::make_pair(tf, btn);
}
