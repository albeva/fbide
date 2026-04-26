//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SideBarManager.hpp"
#include "app/Context.hpp"
#include "document/DocumentManager.hpp"
#include <wx/dirctrl.h>
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

    m_dirCtrl = make_unowned<wxGenericDirCtrl>(
        m_notebook,
        wxID_ANY,
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
