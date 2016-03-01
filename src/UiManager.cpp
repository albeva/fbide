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

using namespace fbide;


// event dispatching
BEGIN_EVENT_TABLE(UiManager, wxEvtHandler)
    EVT_CLOSE(UiManager::OnClose)
wxEND_EVENT_TABLE()


// Instantiate classes, but specific
// loading (especially dependant on configuration)
// is deffered to Load method
UiManager::UiManager()
{
    // the frame
    m_window = new MainWindow(nullptr, wxID_ANY, "fbide");
    m_window->PushEventHandler(this);
    wxTheApp->SetTopWindow(m_window);
    
    // aui
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT);
    m_aui.SetManagedWindow(m_window);
    m_aui.Update();
    
    // main doc area
    m_docArea = new wxAuiNotebook(m_window,
                                  wxID_ANY,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_WINDOWLIST_BUTTON);
    m_aui.AddPane(m_docArea, wxAuiPaneInfo()
                  .Name("fbideDocArea")
                  .CenterPane()
                  .PaneBorder(false));
}


// shut down the UI
UiManager::~UiManager()
{
    // in theory should do `delete m_window;` ?
    // not sure though. sometimes gives an error
    // I think window is automatically deleted so ...
    // no need to do that manyally.
}


// Load the UI
void UiManager::Load()
{
    m_docArea->AddPage(new wxStyledTextCtrl(m_docArea), "One");
    m_docArea->AddPage(new wxStyledTextCtrl(m_docArea), "Two");
}


// window is about to close
void UiManager::OnClose(wxCloseEvent & event)
{
    m_aui.UnInit();
    m_window->RemoveEventHandler(this);
    m_window->Destroy();
}
