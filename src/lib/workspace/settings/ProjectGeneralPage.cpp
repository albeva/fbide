//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectGeneralPage.hpp"
#include "app/Context.hpp"
#include "workspace/Project.hpp"
using namespace fbide;

ProjectGeneralPage::ProjectGeneralPage(Context& ctx, wxWindow* parent, Project& project)
: Panel(ctx, wxID_ANY, parent)
, m_project(project) {}

void ProjectGeneralPage::create() {
    m_name = m_project.getName();
    hbox({ .alignment = SmartBoxSizer::Alignment::Center }, [&] {
        text(getContext().tr("project.settings.name"), { .expand = false });
        m_nameField = textField(m_name, { .proportion = 1 });
    });
    SetSizerAndFit(currentSizer());
}

auto ProjectGeneralPage::validate() -> bool {
    auto name = m_name;
    name.Trim().Trim(false);
    if (name.IsEmpty()) {
        m_nameField->SetFocus();
        wxMessageBox(
            getContext().tr("project.settings.nameRequired"),
            getContext().tr("project.settings.title"),
            wxICON_WARNING | wxOK, this
        );
        return false;
    }
    return true;
}

void ProjectGeneralPage::apply() {
    auto name = m_name;
    name.Trim().Trim(false);
    m_project.setName(name);
    m_project.save();
}
