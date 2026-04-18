//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SettingsDialog.hpp"
#include "app/Context.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
#include "ui/UIManager.hpp"
#include "panels/CompilerPage.hpp"
#include "panels/GeneralPage.hpp"
#include "panels/KeywordsPage.hpp"
#include "panels/ThemePage.hpp"
using namespace fbide;

SettingsDialog::SettingsDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, ctx.tr("dialogs.settings.title"),
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
  )
, m_ctx(ctx) {}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::create() {
    const auto notebook = make_unowned<wxNotebook>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

    m_generalPage = make_unowned<GeneralPage>(m_ctx, notebook);
    m_themePage = make_unowned<ThemePage>(m_ctx, notebook);
    m_keywordsPage = make_unowned<KeywordsPage>(m_ctx, notebook);
    m_compilerPage = make_unowned<CompilerPage>(m_ctx, notebook);

    m_generalPage->create();
    m_themePage->create();
    m_keywordsPage->create();
    m_compilerPage->create();

    notebook->AddPage(m_generalPage, m_ctx.tr("dialogs.settings.general.title"));
    notebook->AddPage(m_themePage, m_ctx.tr("dialogs.settings.themes.title"));
    notebook->AddPage(m_keywordsPage, m_ctx.tr("dialogs.settings.keywords.title"));
    notebook->AddPage(m_compilerPage, m_ctx.tr("dialogs.settings.compiler.title"));

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizerAndFit(mainSizer);
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
    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
    m_ctx.getUIManager().updateEditorSettigs();
}
