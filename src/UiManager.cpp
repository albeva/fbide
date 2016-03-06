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

using namespace fbide;


// event dispatching
BEGIN_EVENT_TABLE(UiManager, wxEvtHandler)
wxEND_EVENT_TABLE()


// Instantiate classes, but specific
// loading (especially dependant on configuration)
// is deffered to Load method
UiManager::UiManager()
{
    // the frame
    m_window = std::make_unique<MainWindow>(nullptr, wxID_ANY, "fbide");
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
                                  wxID_ANY,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_WINDOWLIST_BUTTON);
    m_aui.AddPane(m_docArea, wxAuiPaneInfo()
                  .Name("fbideDocArea")
                  .CenterPane()
                  .PaneBorder(false));
    

}


// shut down the UI. Clean things
// up in proper order
UiManager::~UiManager()
{
    m_aui.UnInit();
    m_window->RemoveEventHandler(this);
}


// Load the UI
void UiManager::Load()
{
    auto & conf = GetCfgMgr().Get();
    
    m_tbarHandler->Load(conf["Ui.Toolbars"]);
    m_menuHandler->Load(conf["Ui.Menus"]);
    
    m_aui.Update();
    
    m_docArea->AddPage(new wxStyledTextCtrl(m_docArea), "One");
    m_docArea->AddPage(new wxStyledTextCtrl(m_docArea), "Two");
}


/**
 * Set art provider
 */
void UiManager::SetArtProvider(IArtProvider * artProvider)
{
    m_artProvider = std::unique_ptr<IArtProvider>{artProvider};
}
