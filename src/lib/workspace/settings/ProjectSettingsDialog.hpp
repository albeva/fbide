//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Layout.hpp"

namespace fbide {
class Context;
class Project;

/// Modal dialog for per-project settings. Currently a placeholder — the actual
/// settings panels (build targets, etc.) land in a later pass.
class ProjectSettingsDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(ProjectSettingsDialog)

    /// Construct without populating widgets; `create()` builds the UI.
    ProjectSettingsDialog(wxWindow* parent, Context& ctx, Project& project);
    /// Build the dialog widgets.
    void create();

private:
    Context& m_ctx;     ///< Application context.
    Project& m_project; ///< The project being configured.
};

} // namespace fbide
