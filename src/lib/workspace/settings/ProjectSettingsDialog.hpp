//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
class Project;
class ProjectGeneralPage;

/// Modal dialog for per-project settings — a notebook of setting pages
/// (currently just General). Edits are applied on OK and persisted to the
/// project's `.fbp`.
class ProjectSettingsDialog final : public wxDialog {
public:
    NO_COPY_AND_MOVE(ProjectSettingsDialog)

    /// Construct without populating panels; `create()` builds them.
    ProjectSettingsDialog(wxWindow* parent, Context& ctx, Project& project);
    /// Out-of-line so the destructor sees the page definitions.
    ~ProjectSettingsDialog() override;
    /// Build the notebook + pages and finalize layout.
    void create();

private:
    /// Run every page's `apply()`. Returns `false` (keeping the dialog open,
    /// offending page selected) when a page rejects its input.
    [[nodiscard]] auto applyChanges() -> bool;

    Context& m_ctx;                            ///< Application context.
    Project& m_project;                        ///< The project being configured.
    Unowned<wxNotebook> m_notebook;            ///< Tab notebook owning the pages.
    Unowned<ProjectGeneralPage> m_generalPage; ///< General tab.
};

} // namespace fbide
