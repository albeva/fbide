//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectSettingsDialog.hpp"
#include "app/Context.hpp"
#include "workspace/Project.hpp"
using namespace fbide;

ProjectSettingsDialog::ProjectSettingsDialog(wxWindow* parent, Context& ctx, Project& project)
: Layout(
      parent, wxID_ANY, ctx.tr("project.settings.title"),
      wxDefaultPosition, wxSize(420, 280),
      wxDEFAULT_DIALOG_STYLE
  )
, m_ctx(ctx)
, m_project(project) {}

void ProjectSettingsDialog::create() {
    // Placeholder until the real per-project settings panels are added.
    label(wxString::Format(m_ctx.tr("project.settings.placeholder"), m_project.getName()));
    add(CreateStdDialogButtonSizer(wxOK | wxCANCEL));
    SetSizerAndFit(currentSizer());
    Centre();
}
