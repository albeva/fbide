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
    class IArtProvider;
    class MenuHandler;
    
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
        
        /**
         * Set art provider
         */
        void SetArtProvider(IArtProvider * artProvider);
        
        /**
         * Get the art provider
         */
        inline IArtProvider & GetArtProvider() const { return *m_artProvider; }
        
        
    private:
        
        void OnClose(wxCloseEvent & event);
        
        MainWindow    * m_window;
        wxMenuBar     * m_menu;
        wxAuiManager    m_aui;
        wxAuiNotebook * m_docArea;
        
        std::unique_ptr<IArtProvider> m_artProvider{nullptr};
        std::unique_ptr<MenuHandler> m_menuHandler{nullptr};
        
        wxDECLARE_EVENT_TABLE();
        DECLARE_MANAGER();
    };
    
}
