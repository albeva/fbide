//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Sidebar view of the open persistent project's file / folder tree.
 *
 * Lives as a page in the Browser sidebar notebook. `SideBarManager`
 * inserts it when a persistent `Project` is loaded and deletes it when
 * the project closes, so the view only exists while there is a project
 * to show.
 *
 * **Empty for now** — the tree control and project binding land in a
 * later phase; this is just the docked container.
 */
class ProjectTreeView final : public wxPanel {
public:
    NO_COPY_AND_MOVE(ProjectTreeView)

    /// Construct parented to the sidebar notebook. No content yet.
    explicit ProjectTreeView(wxWindow* parent);
};

} // namespace fbide
