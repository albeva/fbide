//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

class wxGenericDirCtrl;
class wxTreeCtrl;
class wxTreeEvent;

namespace fbide {
class Context;
class Document;
class SymbolTable;

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

    /// Update the Sub/Function tree to show `doc`'s current SymbolTable.
    /// Pass `nullptr` (or a doc with no parsed table yet) to clear the tree.
    /// Caller responsibilities: invoke when the active document changes
    /// (tab switch) and when the active document's SymbolTable changes
    /// (intellisense result with a new hash). Internally short-circuits
    /// when the same shared_ptr<SymbolTable> is shown twice in a row.
    void showSymbolsFor(const Document* doc);

    /// Reveal the Browser pane (toggling its CommandEntry on if hidden) and
    /// switch the notebook to the Sub/Function tree tab. Bound to the
    /// "Show Subs" command (F2).
    void showSymbolBrowser();

private:
    void onFileActivated(wxTreeEvent& event);
    void onSymbolItemActivated(wxTreeEvent& event);
    void rebuildSymbolTree(const SymbolTable& table);
    void clearSymbolTree();

    Context& m_ctx;
    wxAuiNotebook* m_notebook = nullptr;
    Unowned<wxGenericDirCtrl> m_dirCtrl;
    Unowned<wxTreeCtrl> m_symbolTree;
    int m_subFunctionPage = wxNOT_FOUND;
    int m_browseFilesPage = wxNOT_FOUND;
    /// Last SymbolTable rendered into the tree. Used as a cheap dedup check —
    /// repeated showSymbolsFor calls with the same shared_ptr are no-ops.
    std::shared_ptr<const SymbolTable> m_currentTable;
};

} // namespace fbide
