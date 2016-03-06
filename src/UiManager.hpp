//
//  UiManager.hpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#pragma once

namespace fbide {
    
    class MainWindow;
    class IArtProvider;
    class MenuHandler;
    class ToolbarHandler;
    
    /**
     * Manage fbide UI.
     * app frame, menus, toolbars and panels
     */
    class UiManager : NonCopyable, public wxEvtHandler
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
        inline MainWindow * GetWindow() { return m_window.get(); }
        
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
        
        // life of these is tied to main window. So they are just pointers
        wxMenuBar     * m_menu;
        wxAuiManager    m_aui;
        wxAuiNotebook * m_docArea;
        
        // managed resources
        std::unique_ptr<MainWindow>     m_window;
        std::unique_ptr<IArtProvider>   m_artProvider;
        std::unique_ptr<MenuHandler>    m_menuHandler;
        std::unique_ptr<ToolbarHandler> m_tbarHandler;
        
        wxDECLARE_EVENT_TABLE();
    };
    
}
