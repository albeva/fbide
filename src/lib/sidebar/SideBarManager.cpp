//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include "FileBrowser.hpp"
#include "SymbolBrowserPanel.hpp"
#include <wx/config.h>
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "document/FileSession.hpp"
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

auto SideBarManager::activeTab() const -> wxString {
    if (m_notebook == nullptr) {
        return {};
    }
    const int sel = m_notebook->GetSelection();
    if (sel == m_subFunctionPage) {
        return "subFunction";
    }
    if (sel == m_browseFilesPage) {
        return "browseFiles";
    }
    return {};
}

void SideBarManager::setActiveTab(const wxString& key) {
    if (m_notebook == nullptr) {
        return;
    }
    int page = wxNOT_FOUND;
    if (key == "subFunction") {
        page = m_subFunctionPage;
    } else if (key == "browseFiles") {
        page = m_browseFilesPage;
    }
    if (page != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(page));
    }
}

void SideBarManager::store(FileSession& session) const {
    if (m_fileBrowser != nullptr) {
        m_fileBrowser->store(session);
    }
    auto& config = session.getConfig();
    config.SetPath("/sidebar");
    config.Write("activeTab", activeTab());
}

void SideBarManager::load(FileSession& session) {
    // Restore the browser tree first, then select the tab: switching to Browse
    // Files re-establishes its filesystem watch on the restored expansion.
    if (m_fileBrowser != nullptr) {
        m_fileBrowser->load(session);
    }
    auto& config = session.getConfig();
    config.SetPath("/sidebar");
    if (wxString tab; config.Read("activeTab", &tab)) {
        setActiveTab(tab);
    }
}
