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


// Load user interface
UiManager::UiManager()
{
    m_window = new MainWindow(nullptr, wxID_ANY, "fbide");
    m_window->PushEventHandler(this);
    wxTheApp->SetTopWindow(m_window);
}


// shut down the UI
UiManager::~UiManager()
{
    if (m_window == nullptr) return;
    
    // destroy
    if (!m_window->IsBeingDeleted()) {
        delete m_window; // will this caouse recursive call?
    }
    
    m_window = nullptr;
}


// Load the UI
void UiManager::Load()
{
}


// window is about to close
void UiManager::OnClose(wxCloseEvent & event)
{
    m_window->RemoveEventHandler(this);
    m_window->Destroy();
}
