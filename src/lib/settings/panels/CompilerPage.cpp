//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "CompilerPage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

namespace {
constexpr int border = 5;
}

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {}

void CompilerPage::layout() {
    const auto& lang = getContext().getLang();
    const auto& config = getContext().getConfig();

    auto makeFieldBox = [&](const wxString& label, const wxString& value, Unowned<wxTextCtrl>& ctrl, wxButton** browseBtn = nullptr) {
        const auto box = make_unowned<wxStaticBoxSizer>(wxVERTICAL, this, label);
        const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
        ctrl = make_unowned<wxTextCtrl>(this, wxID_ANY, value);
        row->Add(ctrl.get(), 1, wxEXPAND);
        if (browseBtn != nullptr) {
            *browseBtn = make_unowned<wxButton>(this, wxID_ANY, "...", wxDefaultPosition, wxSize(30, -1));
            row->Add(*browseBtn, 0, wxLEFT, border);
        }
        box->Add(row, 0, wxEXPAND | wxALL, border);
        return box;
    };

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::SettingsCompilerAndPaths]), 0, wxALL, border);

    wxButton* btnCompiler = nullptr;
    sizer->Add(makeFieldBox(lang[LangId::SettingsCompilerPath], config.getCompilerPath(), m_textCompilerPath, &btnCompiler), 0, wxEXPAND | wxALL, border);
    sizer->Add(makeFieldBox(lang[LangId::SettingsCompilerCommand], config.getCompileCommand(), m_textCompileCommand), 0, wxEXPAND | wxALL, border);
    sizer->Add(makeFieldBox(lang[LangId::SettingsRunCmd], config.getRunCommand(), m_textRunCommand), 0, wxEXPAND | wxALL, border);

    wxButton* btnHelp = nullptr;
    sizer->Add(makeFieldBox(lang[LangId::SettingsHelpFile], config.getHelpFile(), m_textHelpFile, &btnHelp), 0, wxEXPAND | wxALL, border);

    SetSizer(sizer);

    btnCompiler->Bind(wxEVT_BUTTON, &CompilerPage::onCompilerBrowse, this);
    btnHelp->Bind(wxEVT_BUTTON, &CompilerPage::onHelpBrowse, this);
}

void CompilerPage::apply() {
    auto& config = getContext().getConfig();
    config.setCompilerPath(m_textCompilerPath->GetValue());
    config.setCompileCommand(m_textCompileCommand->GetValue());
    config.setRunCommand(m_textRunCommand->GetValue());
    config.setHelpFile(m_textHelpFile->GetValue());
}

void CompilerPage::onCompilerBrowse(wxCommandEvent&) {
    wxFileDialog dlg(this, "Select compiler", "", "",
#ifdef __WXMSW__
        "FreeBASIC (fbc.exe)|fbc.exe|All programs (*.exe)|*.exe",
#else
        "All files (*)|*",
#endif
        wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_textCompilerPath->SetValue(dlg.GetPath());
    }
}

void CompilerPage::onHelpBrowse(wxCommandEvent&) {
    wxFileDialog dlg(this, "Select help file", "", "",
        getContext().getLang()[LangId::SettingsHelpFileFilter] + "|*.chm",
        wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_textHelpFile->SetValue(dlg.GetPath());
    }
}
