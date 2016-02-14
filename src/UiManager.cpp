//
//  UiManager.cpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#ifdef _MSC_VER
    #include "app_pch.hpp"
#endif

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
    // in theory should do `delete m_window;` ?
    // not sure though. sometimes gives an error
    // I think window is automatically deleted so ...
    // no need to do that manyally.
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
