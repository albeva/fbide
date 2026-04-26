//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

class wxGenericDirCtrl;
class wxTreeEvent;

namespace fbide {
class Context;

/// Manages the sidebar (Browser) pane content. The pane and its notebook are
/// owned by `UIManager`; SideBarManager populates the notebook with tabs and
/// drives their behaviour.
///
/// First cut: a single "Browse Files" tab wrapping wxGenericDirCtrl. The
/// notebook is ready for additional tabs (Sub/Function tree, list) later.
class SideBarManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(SideBarManager)

    /// AUI pane name for the Browser sidebar. Must match the CommandEntry name
    /// for `CommandId::Browser` so toggling shows/hides the pane.
    static constexpr auto kBrowserPaneName = "viewBrowser";

    explicit SideBarManager(Context& ctx);
    ~SideBarManager() override;

    /// Two-phase init: called from `UIManager::createLayout` once the sidebar
    /// notebook exists. Builds the tabs.
    void attach(wxAuiNotebook* notebook);

    /// Switch to the Browse Files tab and reveal `path` in the directory tree.
    /// No-op if `path` is empty or `attach` has not been called.
    void locateFile(const wxString& path);

private:
    void onFileActivated(wxTreeEvent& event);

    Context& m_ctx;
    wxAuiNotebook* m_notebook = nullptr;
    Unowned<wxGenericDirCtrl> m_dirCtrl;
    int m_browseFilesPage = wxNOT_FOUND;
};

} // namespace fbide
