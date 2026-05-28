//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SettingsDialog.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "panels/CompilerPage.hpp"
#include "panels/GeneralPage.hpp"
#include "panels/KeywordsPage.hpp"
#include "panels/ThemePage.hpp"
#include "ui/UIManager.hpp"
#include "ui/controls/Panel.hpp"
using namespace fbide;

SettingsDialog::SettingsDialog(wxWindow* parent, Context& ctx)
: wxDialog(
      parent, wxID_ANY, ctx.tr("dialogs.settings.title"),
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
  )
, m_ctx(ctx) {}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::create(const Page initial) {
    m_notebook = make_unowned<wxNotebook>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

    m_generalPage = make_unowned<GeneralPage>(m_ctx, m_notebook);
    m_themePage = make_unowned<ThemePage>(m_ctx, m_notebook);
    m_keywordsPage = make_unowned<KeywordsPage>(m_ctx, m_notebook);
    m_compilerPage = make_unowned<CompilerPage>(m_ctx, m_notebook);

    m_generalPage->create();
    m_themePage->create();
    m_keywordsPage->create();
    m_compilerPage->create();

    m_notebook->AddPage(m_generalPage, m_ctx.tr("dialogs.settings.general.title"));
    m_notebook->AddPage(m_themePage, m_ctx.tr("dialogs.settings.themes.title"));
    m_notebook->AddPage(m_keywordsPage, m_ctx.tr("dialogs.settings.keywords.title"));
    m_notebook->AddPage(m_compilerPage, m_ctx.tr("dialogs.settings.compiler.title"));
    m_notebook->SetSelection(static_cast<std::size_t>(initial));

    // Defer initial-page focus until the dialog is shown — calling
    // SetFocus on a not-yet-realised control is a no-op on Windows.
    if (initial == Page::Compiler) {
        CallAfter([this]() { m_compilerPage->focusCompilerPath(); });
    }

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizerAndFit(mainSizer);
    Centre();

    Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&) {
            if (applyChanges()) {
                EndModal(wxID_OK);
            }
        },
        wxID_OK
    );
    Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&) {
            cancelChanges();
            EndModal(wxID_CANCEL);
        },
        wxID_CANCEL
    );
    // Title-bar close = Cancel — restore on the way out.
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        cancelChanges();
        event.Skip();
    });
}

auto SettingsDialog::applyChanges() -> bool {
    const auto thaw = m_ctx.getUIManager().freeze();

    // Apply order: currently-selected tab first so any validation
    // failure surfaces against the page the user has in front of them.
    // The remaining tabs follow in declaration order.
    const auto activePage = static_cast<Page>(m_notebook->GetSelection());
    constexpr std::array<Page, 4> kOrder {
        Page::General, Page::Theme, Page::Keywords, Page::Compiler
    };

    auto applyOne = [this](Page page) -> bool {
        auto* panel = panelAt(page);
        if (panel != nullptr && !panel->apply()) {
            m_notebook->SetSelection(static_cast<std::size_t>(page));
            return false;
        }
        return true;
    };

    if (!applyOne(activePage)) {
        return false;
    }
    for (auto page : kOrder) {
        if (page == activePage) {
            continue;
        }
        if (!applyOne(page)) {
            return false;
        }
    }

    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
    m_ctx.getUIManager().updateSettings();
    return true;
}

void SettingsDialog::cancelChanges() const {
    m_generalPage->cancel();
    m_themePage->cancel();
    m_keywordsPage->cancel();
    m_compilerPage->cancel();
}

auto SettingsDialog::panelAt(const Page page) const -> Panel* {
    switch (page) {
    case Page::General:
        return m_generalPage;
    case Page::Theme:
        return m_themePage;
    case Page::Keywords:
        return m_keywordsPage;
    case Page::Compiler:
        return m_compilerPage;
    }
    return nullptr;
}
