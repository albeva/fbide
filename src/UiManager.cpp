//
//  UiManager.cpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "UiManager.hpp"
#include "MainWindow.hpp"
#include "ConfigManager.hpp"

#include "MenuHandler.hpp"
#include "ToolbarHandler.hpp"
#include "StandardArtProvider.hpp"
#include "CmdManager.hpp"
#include "TypeManager.hpp"
#include "Document.hpp"

using namespace fbide;

namespace {
    const int ID_AppWindow   = ::wxNewId();
    const int ID_FullScreen  = ::wxNewId();
    const int ID_DocNotebook = ::wxNewId();
}

// event dispatching
wxBEGIN_EVENT_TABLE(UiManager, wxEvtHandler)
    EVT_MENU                    (wxID_ANY,          UiManager::HandleMenuEvents)
    EVT_MENU                    (wxID_NEW,          UiManager::OnNew)
    EVT_MENU                    (wxID_OPEN,         UiManager::OnOpen)
    EVT_MENU                    (wxID_SAVE,         UiManager::OnSave)
    EVT_AUINOTEBOOK_PAGE_CLOSE  (ID_DocNotebook,    UiManager::OnPaneClose)
wxEND_EVENT_TABLE()


// Instantiate classes, but specific
// loading (especially dependant on configuration)
// is deffered to Load method
UiManager::UiManager()
{
    // the frame
    m_window = std::make_unique<MainWindow>(nullptr, ID_AppWindow, "fbide");
    m_window->PushEventHandler(this);
    wxTheApp->SetTopWindow(m_window.get());

    // Load default the art provider
    m_artProvider = std::make_unique<StandardArtProvider>();
    
    // the menu
    m_menu = new wxMenuBar();
    m_window->SetMenuBar(m_menu);
    
    // menu handler
    m_menuHandler = std::make_unique<MenuHandler>(m_menu);
    
    // aui
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT);
    m_aui.SetManagedWindow(m_window.get());
    m_aui.Update();
    
    // toolbar handler
    m_tbarHandler = std::make_unique<ToolbarHandler>(&m_aui);
    
    // main doc area
    m_docArea = new wxAuiNotebook(m_window.get(),
                                  ID_DocNotebook,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_WINDOWLIST_BUTTON | wxAUI_NB_CLOSE_ON_ALL_TABS);
    m_aui.AddPane(m_docArea, wxAuiPaneInfo()
                  .Name("fbideDocArea")
                  .CenterPane()
                  .PaneBorder(false));
    
    // toggle fullscreen
    GetCmdMgr().Register("fullscreen", {ID_FullScreen, CmdManager::Type::Check, false});
}


// shut down the UI. Clean things
// up in proper order
UiManager::~UiManager()
{
    m_aui.UnInit();
    m_window->RemoveEventHandler(this);
}


/**
 * Shotdown the manager
 */
void UiManager::Unload()
{
    // close any pending documents
    while (m_docArea->GetPageCount()) {
        CloseTab(0);
    }
}


// Load the UI
void UiManager::Load()
{
    auto & layoutCfg = GetConfig("Layout");
	m_menuHandler->Load(layoutCfg["Menus"]);
    m_tbarHandler->Load(layoutCfg["Toolbars"]);
    m_aui.Update();
}


/**
 * Set art provider
 */
void UiManager::SetArtProvider(IArtProvider * artProvider)
{
    m_artProvider = std::unique_ptr<IArtProvider>{artProvider};
}


/**
 * Handle command events
 */
void UiManager::HandleMenuEvents(wxCommandEvent & event)
{
    // allow others to catch
    event.Skip();
    
    // let CmdMagr check the status (if this is a registered check)
    GetCmdMgr().Check(event.GetId(), event.IsChecked());
    
    if (event.GetId() == ID_FullScreen) {
        m_window->ShowFullScreen(event.IsChecked());
    }
}


/**
 * Open new blank default document
 */
void UiManager::OnNew(wxCommandEvent & event)
{
    wxWindowUpdateLocker lock{m_window.get()};
    auto & type = GetTypeMgr();
    auto doc = type.CreateFromType("default");
    doc->Create();
}


/**
 * Close AUI pane
 */
void UiManager::OnPaneClose(wxAuiNotebookEvent & event)
{
    event.Veto();
    auto index = event.GetSelection();
    CloseTab(index);
}


void UiManager::CloseTab(size_t index)
{
    auto wnd = m_docArea->GetPage(index);
    auto iter = m_closers.find(wnd);
    if (iter != m_closers.end()) {
        iter->second();
        m_closers.erase(iter);
    } else {
        m_docArea->DeletePage(index);
    }
}


/**
 * Show open file dialog
 */
void UiManager::OnOpen(wxCommandEvent & event)
{
    
}


/**
 * Save currently active document
 */
void UiManager::OnSave(wxCommandEvent & event)
{
    
}
