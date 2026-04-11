//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SettingsDialog.hpp"
#include "panels/CompilerPage.hpp"
#include "panels/GeneralPage.hpp"
#include "panels/KeywordsPage.hpp"
#include "panels/ThemePage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr int border = 5;
}

SettingsDialog::SettingsDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, ctx.getLang()[LangId::SettingsTitle],
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
  )
, m_ctx(ctx) {}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::create() {
    const auto& lang = m_ctx.getLang();
    const auto notebook = make_unowned<wxNotebook>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

    m_generalPage = make_unowned<GeneralPage>(m_ctx, notebook);
    m_themePage = make_unowned<ThemePage>(m_ctx, notebook);
    m_keywordsPage = make_unowned<KeywordsPage>(m_ctx, notebook);
    m_compilerPage = make_unowned<CompilerPage>(m_ctx, notebook);

    m_generalPage->layout();
    m_themePage->layout();
    m_keywordsPage->layout();
    m_compilerPage->layout();

    notebook->AddPage(m_generalPage, lang[LangId::SettingsGeneral]);
    notebook->AddPage(m_themePage, lang[LangId::SettingsThemes]);
    notebook->AddPage(m_keywordsPage, lang[LangId::SettingsKeywords]);
    notebook->AddPage(m_compilerPage, lang[LangId::SettingsCompiler]);

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, border);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);
    SetSizer(mainSizer);
    SetMinSize(wxSize(500, 400));
    Fit();
    Centre();

    Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&) {
            applyChanges();
            EndModal(wxID_OK);
        },
        wxID_OK
    );
}

void SettingsDialog::applyChanges() const {
    m_generalPage->apply();
    m_themePage->apply();
    m_keywordsPage->apply();
    m_compilerPage->apply();

    m_ctx.getConfig().save();

    // Reapply settings to all open editors
    const auto* notebook = m_ctx.getUIManager().getNotebook();
    for (size_t idx = 0; idx < notebook->GetPageCount(); idx++) {
        if (auto* editor = dynamic_cast<Editor*>(notebook->GetPage(idx))) {
            editor->applySettings();
        }
    }
}
