//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectSettingsDialog.hpp"
#include "ProjectGeneralPage.hpp"
#include "app/Context.hpp"
#include "workspace/Project.hpp"
using namespace fbide;

ProjectSettingsDialog::ProjectSettingsDialog(wxWindow* parent, Context& ctx, Project& project)
: wxDialog(
      parent, wxID_ANY, ctx.tr("project.settings.title"),
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
  )
, m_ctx(ctx)
, m_project(project) {}

ProjectSettingsDialog::~ProjectSettingsDialog() = default;

void ProjectSettingsDialog::create() {
    m_notebook = make_unowned<wxNotebook>(this, wxID_ANY);

    m_generalPage = make_unowned<ProjectGeneralPage>(m_ctx, m_notebook.get(), m_project);
    m_generalPage->create();
    m_notebook->AddPage(m_generalPage, m_ctx.tr("project.settings.general"));

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
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
        [this](wxCommandEvent&) { EndModal(wxID_CANCEL); },
        wxID_CANCEL
    );
}

auto ProjectSettingsDialog::applyChanges() -> bool {
    // Single page today; iterate the notebook (active page first) when more land.
    if (!m_generalPage->validate()) {
        return false;
    }
    m_generalPage->apply();
    return true;
}
