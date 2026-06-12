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
class Document;
class FileBrowser;
class Project;
class SymbolBrowserPanel;
class ProjectTreeView;

/**
 * Populates the Browser sidebar notebook with tabs (Browse Files,
 * Sub/Function tree) and drives their behaviour.
 *
 * **Owns:** the tab content (wx-parented to the notebook) — the
 * `FileBrowser` and `SymbolBrowserPanel`. It is a thin tab manager: each
 * tab owns its own behaviour (the `FileBrowser` drives the filesystem tree
 * and its watch; this class only toggles that watch with the sidebar's
 * visibility).
 * **Owned by:** `Context`. Declared *after* `UIManager` so destruction
 * runs first — its non-owning pointer to the sidebar `wxAuiNotebook`
 * (owned by the frame UIManager destroys) cannot dangle.
 * **Threading:** UI thread only.
 * **Init:** two-phase. Constructor runs early during Context build;
 * `attach(notebook)` lands later from `UIManager::createLayout`
 * once the notebook exists.
 *
 * See @ref ui and @ref architecture.
 */
class SideBarManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(SideBarManager)

    /// AUI pane name for the Browser sidebar. Must match the CommandEntry name
    /// for `CommandId::Browser` so toggling shows/hides the pane.
    static constexpr auto kBrowserPaneName = "viewBrowser";

    /// Construct without populating tabs; `attach()` does that later.
    explicit SideBarManager(Context& ctx);
    /// Out-of-line so the destructor sees the full `SymbolBrowserPanel` definition.
    ~SideBarManager() override;

    /// Two-phase init: called from `UIManager::createLayout` once the sidebar
    /// notebook exists. Builds the tabs.
    void attach(wxAuiNotebook* notebook);

    /// Switch to the Browse Files tab and reveal `path` in the directory tree.
    /// No-op if `path` is empty or `attach` has not been called.
    void locateFile(const wxString& path);

    /// Update the Sub/Function tree to show `doc`'s current SymbolTable.
    /// Pass `nullptr` (or a doc with no parsed table yet) to clear the tree.
    /// Caller responsibilities: invoke when the active document changes
    /// (tab switch) and when the active document's SymbolTable changes
    /// (intellisense result with a new hash). Internally short-circuits
    /// when the same shared_ptr<SymbolTable> is shown twice in a row.
    void showSymbolsFor(const Document* doc);

    /// Reveal the Browser pane (toggling its CommandEntry on if hidden),
    /// switch the notebook to the Sub/Function tree tab and focus its filter
    /// box. Bound to the "Show Subs" command (F2).
    void showSymbolBrowser();

    /// Insert a tree view of `project` as the first sidebar tab, select it,
    /// and reveal the Browser pane. Called by `WorkspaceManager` when a
    /// persistent project is loaded.
    void showProjectTree(Project& project);

    /// Delete the project tree view tab (destroying the view). Called by
    /// `WorkspaceManager` when the persistent project closes. No-op when
    /// no project tree is shown.
    void hideProjectTree();

    /// Capture the project tree's expanded folders + selected node into the
    /// project session. No-op when no project tree is shown. Called by
    /// `WorkspaceManager::saveProjectSession`.
    void captureProjectSession();

private:
    Context& m_ctx;                            ///< Application context.
    wxAuiNotebook* m_notebook = nullptr;       ///< Non-owning pointer into the sidebar notebook (owned by the frame).
    Unowned<FileBrowser> m_fileBrowser;        ///< Browse Files tab (self-manages its filesystem watch).
    Unowned<SymbolBrowserPanel> m_symbolPanel; ///< Sub/Function tab (filter box + tree).
    Unowned<ProjectTreeView> m_projectTree;    ///< Project tree tab — present only while a project is open.
};

} // namespace fbide
