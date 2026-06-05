//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {
class Context;
class Project;

/// General project-settings page. Currently just the project's display name;
/// more project settings land here (or on sibling pages) later.
class ProjectGeneralPage final : public Panel {
public:
    NO_COPY_AND_MOVE(ProjectGeneralPage)

    /// Construct without populating widgets; `create()` builds the UI.
    ProjectGeneralPage(Context& ctx, wxWindow* parent, Project& project);
    /// Build the page widgets.
    void create() override;
    /// Validate + persist the name to the project's `.fbp`. Returns `false`
    /// (keeping the dialog open, field focused) when the name is empty.
    auto apply() -> bool override;

private:
    Project& m_project;              ///< The project being configured.
    wxString m_name;                 ///< Bound to the name field; committed on apply().
    Unowned<wxTextCtrl> m_nameField; ///< Held to focus on a validation error.
};

} // namespace fbide
