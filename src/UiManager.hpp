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
         * Get main window
         */
        inline MainWindow * GetWindow() { return m_window; }
        
        
    private:
        
        void OnClose(wxCloseEvent & event);
        
        MainWindow * m_window;
        
        wxDECLARE_EVENT_TABLE();
        DECLARE_MANAGER();
    };
    
}
