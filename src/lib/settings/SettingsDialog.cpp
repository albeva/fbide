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

void SettingsDialog::create(const wxString& target) {
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
    const auto page = pageFromName(target.BeforeFirst('/'));
    const wxString subPath = target.AfterFirst('/');
    m_notebook->SetSelection(static_cast<std::size_t>(page));

    // Defer focus until the dialog is shown — calling SetFocus on a
    // not-yet-realised control is a no-op on Windows. The page decodes
    // the remaining path itself (e.g. "<config-slug>/<field>").
    if (!target.IsEmpty()) {
        CallAfter([this, page, subPath]() {
            if (auto* panel = panelAt(page)) {
                panel->focusPath(subPath);
            }
        });
    }

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);

#ifdef __WXMSW__
    // "Auto detect" shares the button row, left-aligned, and is shown only
    // while the Compiler tab is active. wxID_ANY so it never closes the dialog.
    m_autoDetectButton = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("dialogs.settings.compiler.autoDetect"));
    const auto buttonRow = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    buttonRow->Add(m_autoDetectButton, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    buttonRow->AddStretchSpacer(1);
    buttonRow->Add(btnSizer, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(buttonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    m_autoDetectButton->Show(m_notebook->GetSelection() == static_cast<int>(Page::Compiler));
    m_autoDetectButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        m_compilerPage->autoDetect();
        // Auto-detect may have wired up a bundled manual (paths.helpFile);
        // mirror it into the Keywords tab so the field shows it and its
        // apply() doesn't overwrite it with the value loaded at open time.
        m_keywordsPage->refreshHelpFileFromConfig();
    });
    m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& event) {
        m_autoDetectButton->Show(event.GetSelection() == static_cast<int>(Page::Compiler));
        Layout();
        event.Skip();
    });
#else
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
#endif

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

auto SettingsDialog::applyChanges() const -> bool {
    // Validate every panel before committing anything. The active tab
    // is checked first so a failure surfaces against the page in front
    // of the user; the rest follow in notebook order. No side effects
    // yet — a failure here leaves every panel's config untouched (this
    // is what keeps a Compiler error from half-applying Theme/Keywords
    // or scheduling a restart).
    auto* const activePage = m_notebook->GetCurrentPage();

    const auto validateOne = [this](wxWindow* const win) -> bool {
        auto* const panel = dynamic_cast<Panel*>(win);
        if (panel != nullptr && !panel->validate()) {
            m_notebook->SetSelection(static_cast<std::size_t>(m_notebook->FindPage(panel)));
            return false;
        }
        return true;
    };

    if (!validateOne(activePage)) {
        return false;
    }
    for (auto* const child : m_notebook->GetChildren()) {
        if (child == activePage) {
            continue;
        }
        if (!validateOne(child)) {
            return false;
        }
    }

    // All panels valid — commit. apply() can no longer fail.
    const auto thaw = m_ctx.getUIManager().freeze();
    m_generalPage->apply();
    m_themePage->apply();
    m_keywordsPage->apply();
    m_compilerPage->apply();

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

auto SettingsDialog::pageFromName(const wxString& name) -> Page {
    if (name == "compiler") {
        return Page::Compiler;
    }
    if (name == "theme") {
        return Page::Theme;
    }
    if (name == "keywords") {
        return Page::Keywords;
    }
    return Page::General;
}
