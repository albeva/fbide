//
//  UiManager.hpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#pragma once

#include "Manager.hpp"

namespace fbide {
    
    class MainWindow;
    
    /**
     * Manage fbide UI.
     * app frame, menus, toolbars and panels
     */
    class UiManager : NonCopyable, wxEvtHandler
    {
    public:
        
        UiManager();
        virtual ~UiManager();
        
        /**
         * Load the UI manager
         */
        void Load();
        
        /**
         * Get main window
         */
        inline MainWindow * GetWindow() { return m_window; }
        
        /**
         * Main tab area
         */
        inline wxAuiNotebook * GetDocArea() { return m_docArea; }
        
        
    private:
        
        void OnClose(wxCloseEvent & event);
        
        MainWindow    * m_window;
        wxAuiManager    m_aui;
        wxAuiNotebook * m_docArea;
        
        wxDECLARE_EVENT_TABLE();
        DECLARE_MANAGER();
    };
    
}
