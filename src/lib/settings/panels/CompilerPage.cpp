//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerPage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_compilerPath(getConfig().getCompilerPath())
, m_compileCommand(getConfig().getCompileCommand())
, m_runCommand(getConfig().getRunCommand())
, m_helpFile(getConfig().getHelpFile()) {}

void CompilerPage::layout() {
    makeTitle(LangId::SettingsCompilerAndPaths);

    makeTextField(getVBox(), m_compilerPath, LangId::SettingsCompilerPath, [&] {
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
    });

    makeTextField(getVBox(), m_compileCommand, LangId::SettingsCompilerCommand);
    makeTextField(getVBox(), m_runCommand, LangId::SettingsRunCmd);

    makeTextField(getVBox(), m_helpFile, LangId::SettingsHelpFile, [&] {
        wxFileDialog dlg(
            this, "Select help file", "", "",
            getContext().getLang()[LangId::SettingsHelpFileFilter] + "|*.chm",
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_helpFile = dlg.GetPath();
        }
    });
}

void CompilerPage::apply() {
    auto& config = getConfig();
    config.setCompilerPath(m_compilerPath);
    config.setCompileCommand(m_compileCommand);
    config.setRunCommand(m_runCommand);
    config.setHelpFile(m_helpFile);
}
