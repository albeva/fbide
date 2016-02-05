//
//  UiManager.cpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.h"
#include "UiManager.hpp"
#include "MainWindow.hpp"

using namespace fbide;

// Load user interface
UiManager::UiManager()
{
    m_window = new MainWindow(nullptr, wxID_ANY, "fbide");
    m_window->Show();
}


// shut down the UI
UiManager::~UiManager()
{
    if (m_window != nullptr) {
        if (!m_window->IsBeingDeleted()) {
            delete m_window;
        }
        m_window = nullptr;
    }
}


IMPLEMENT_MANAGER(UiManager)
