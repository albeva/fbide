//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include "FileBrowser.hpp"
#include "ProjectTreeView.hpp"
#include "SymbolBrowserPanel.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "workspace/Project.hpp"
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
    m_notebook->AddPage(m_symbolPanel, m_ctx.tr("sidebar.tabs.subFunction"));

    // Browse Files tab. The FileBrowser owns the directory tree and its
    // filesystem watch, and toggles the watch itself from its own show/hide
    // events — this manager just hosts it as a tab.
    m_fileBrowser = make_unowned<FileBrowser>(m_notebook, m_ctx);
    m_notebook->AddPage(m_fileBrowser, m_ctx.tr("sidebar.tabs.browseFiles"));
}

void SideBarManager::locateFile(const wxString& path) {
    if (path.IsEmpty() || m_fileBrowser == nullptr || m_notebook == nullptr) {
        return;
    }
    if (const int page = m_notebook->GetPageIndex(m_fileBrowser); page != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(page));
    }
    m_fileBrowser->locateFile(path);
}

void SideBarManager::showSymbolBrowser() {
    if (m_notebook == nullptr || m_symbolPanel == nullptr) {
        return;
    }
    // Show the pane: setChecked drives the AUI pane via the CommandEntry's
    // wxAuiManager bind (same path the View → Browser menu uses).
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
        entry->setChecked(true);
    }
    if (const int page = m_notebook->GetPageIndex(m_symbolPanel); page != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(page));
    }
    m_symbolPanel->focusSearch();
}

void SideBarManager::showProjectTree(Project& project) {
    if (m_notebook == nullptr) {
        return;
    }
    // Drop any stale view (close hides first, so this is belt-and-braces).
    if (m_projectTree != nullptr) {
        if (const int page = m_notebook->GetPageIndex(m_projectTree); page != wxNOT_FOUND) {
            m_notebook->DeletePage(static_cast<size_t>(page));
        }
        m_projectTree = nullptr;
    }
    // First tab: the project tree is the primary navigator while a project
    // is open.
    m_projectTree = make_unowned<ProjectTreeView>(m_notebook, m_ctx, project);
    m_notebook->InsertPage(0, m_projectTree, m_ctx.tr("sidebar.tabs.project"), true);
    // Reveal the sidebar pane (same path the View → Browser toggle uses).
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
        entry->setChecked(true);
    }
}

void SideBarManager::hideProjectTree() {
    if (m_notebook == nullptr || m_projectTree == nullptr) {
        return;
    }
    if (const int page = m_notebook->GetPageIndex(m_projectTree); page != wxNOT_FOUND) {
        m_notebook->DeletePage(static_cast<size_t>(page)); // destroys the view window
    }
    m_projectTree = nullptr;
}

void SideBarManager::captureProjectSession() {
    if (m_projectTree != nullptr) {
        m_projectTree->captureSession();
    }
}

void SideBarManager::showSymbolsFor(const Document* doc) {
    if (m_symbolPanel != nullptr) {
        m_symbolPanel->setSymbols(doc);
    }
}
