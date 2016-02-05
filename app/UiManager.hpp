//
//  UiManager.hpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#pragma once

#include "Manager.h"

namespace fbide {
    
    class MainWindow;
    
    /**
     * Manage fbide UI.
     * app frame, menus, toolbars and panels
     */
    class UiManager : NonCopyable
    {
    public:
        
        UiManager();
        virtual ~UiManager();
        
        /**
         * Get main window
         */
        inline MainWindow * GetWindow() const { return m_window; }
        
    private:
        
        MainWindow * m_window;
        
        DECLARE_MANAGER(UiManager)
    };
    
}
