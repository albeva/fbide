//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include "FileBrowser.hpp"
#include "SymbolBrowserPanel.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
using namespace fbide;

SideBarManager::SideBarManager(Context& ctx)
: m_ctx(ctx) {}

SideBarManager::~SideBarManager() = default;

void SideBarManager::attach(wxAuiNotebook* notebook) {
    if (notebook == nullptr) {
        wxLogError("SideBarManager::attach called with null notebook");
        return;
    }
    m_notebook = notebook;

    // Sub/Function tree tab — first page so it matches the old fbide order
    // (S/F tree → Browse Files).
    m_symbolPanel = make_unowned<SymbolBrowserPanel>(m_ctx, m_notebook);
    m_subFunctionPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_symbolPanel, m_ctx.tr("sidebar.tabs.subFunction"));

    // Browse Files tab. The FileBrowser owns the directory tree and its
    // filesystem watch, and toggles the watch itself from its own show/hide
    // events — this manager just hosts it as a tab.
    m_fileBrowser = make_unowned<FileBrowser>(m_notebook, m_ctx);
    m_browseFilesPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_fileBrowser, m_ctx.tr("sidebar.tabs.browseFiles"));
}

void SideBarManager::locateFile(const wxString& path) {
    if (path.IsEmpty() || m_fileBrowser == nullptr || m_notebook == nullptr) {
        return;
    }
    if (m_browseFilesPage != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(m_browseFilesPage));
    }
    m_fileBrowser->locateFile(path);
}

void SideBarManager::showSymbolBrowser() {
    if (m_notebook == nullptr || m_subFunctionPage == wxNOT_FOUND) {
        return;
    }
    // Show the pane: setChecked drives the AUI pane via the CommandEntry's
    // wxAuiManager bind (same path the View → Browser menu uses).
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
        entry->setChecked(true);
    }
    m_notebook->SetSelection(static_cast<size_t>(m_subFunctionPage));
    if (m_symbolPanel != nullptr) {
        m_symbolPanel->focusSearch();
    }
}

void SideBarManager::showSymbolsFor(const Document* doc) {
    if (m_symbolPanel != nullptr) {
        m_symbolPanel->setSymbols(doc);
    }
}
