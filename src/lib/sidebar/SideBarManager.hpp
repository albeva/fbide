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
class Document;
class SymbolBrowser;

/**
 * Populates the Browser sidebar notebook with tabs (Browse Files,
 * Sub/Function tree) and drives their behaviour.
 *
 * **Owns:** the tab content (wx-parented to the notebook) plus the
 * cached `wxGenericDirCtrl*` and `SymbolBrowser*`.
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
    /// Out-of-line so the destructor sees the full `SymbolBrowser` definition.
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
    /// Browse Files tree leaf activated — open the file in a new editor tab.
    void onFileActivated(wxTreeEvent& event);

    Context& m_ctx;                          ///< Application context.
    wxAuiNotebook* m_notebook = nullptr;     ///< Non-owning pointer into the sidebar notebook (owned by the frame).
    Unowned<wxGenericDirCtrl> m_dirCtrl;     ///< Browse Files control.
    Unowned<SymbolBrowser> m_symbolBrowser;  ///< Sub/Function tree control.
    int m_subFunctionPage = wxNOT_FOUND;     ///< Cached page index of the Sub/Function tab.
    int m_browseFilesPage = wxNOT_FOUND;     ///< Cached page index of the Browse Files tab.
};

} // namespace fbide
