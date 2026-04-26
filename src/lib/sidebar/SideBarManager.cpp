//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include <wx/dirctrl.h>
#include "SymbolBrowser.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "document/DocumentManager.hpp"
using namespace fbide;

namespace {
const int BrowserTabsId = wxNewId();
} // namespace

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
    m_symbolBrowser = make_unowned<SymbolBrowser>(m_ctx, m_notebook);
    m_subFunctionPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_symbolBrowser, m_ctx.tr("sidebar.tabs.subFunction"));

    // Browse Files tab.
    m_dirCtrl = make_unowned<wxGenericDirCtrl>(
        m_notebook,
        BrowserTabsId,
        wxDirDialogDefaultFolderStr,
        wxDefaultPosition,
        wxDefaultSize,
        wxDIRCTRL_3D_INTERNAL,
        wxEmptyString
    );
    m_dirCtrl->Bind(wxEVT_DIRCTRL_FILEACTIVATED, &SideBarManager::onFileActivated, this);

    m_browseFilesPage = static_cast<int>(m_notebook->GetPageCount());
    m_notebook->AddPage(m_dirCtrl, m_ctx.tr("sidebar.tabs.browseFiles"));
}

void SideBarManager::locateFile(const wxString& path) {
    if (path.IsEmpty() || m_dirCtrl == nullptr || m_notebook == nullptr) {
        return;
    }
    if (m_browseFilesPage != wxNOT_FOUND) {
        m_notebook->SetSelection(static_cast<size_t>(m_browseFilesPage));
    }
    m_dirCtrl->ExpandPath(path);
    m_dirCtrl->SelectPath(path);
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
    if (m_symbolBrowser != nullptr) {
        m_symbolBrowser->SetFocus();
    }
}

void SideBarManager::showSymbolsFor(const Document* doc) {
    if (m_symbolBrowser != nullptr) {
        m_symbolBrowser->setSymbols(doc);
    }
}

void SideBarManager::onFileActivated(wxTreeEvent& event) {
    event.Skip();
    if (m_dirCtrl == nullptr) {
        return;
    }
    const auto path = m_dirCtrl->GetFilePath();
    if (path.IsEmpty()) {
        return;
    }
    m_ctx.getDocumentManager().openFile(path);
}
