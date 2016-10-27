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
    class UiManager final : NonCopyable, public wxEvtHandler
    {
    public:
        
        /**
         * Cleaner function
         */
        typedef std::function<void()> CloseFn;
        
        UiManager();
        virtual ~UiManager();
        
        /**
         * Load the UI manager
         */
        void Load();
        
        /**
         * Shotdown the manager
         */
        void Unload();
        
        /**
         * Get main window
         */
        inline MainWindow * GetWindow() { return m_window.get(); }
        
        /**
         * Main tab area
         */
        inline wxAuiNotebook * GetDocArea() { return m_docArea; }
        
        /**
         * Bind cleaner function for given wxWindow object
         * Rather than default delete this cleaner function will
         * be invoked instead. Once callback is executed this
         * entry will be removed!
         */
        void BindCloser(wxWindow * wnd, const CloseFn & cb)
        {
            m_closers.emplace(std::make_pair(wnd, cb));
        }
        void BindCloser(wxWindow * wnd, CloseFn && cb)
        {
            m_closers.emplace(std::make_pair(wnd, std::move(cb)));
        }
        
        /**
         * Set art provider
         */
        void SetArtProvider(IArtProvider * artProvider);
        
        /**
         * Get the art provider
         */
        inline IArtProvider & GetArtProvider() const { return *m_artProvider; }
        
    private:
        
        void HandleMenuEvents(wxCommandEvent & event);
        void OnNew(wxCommandEvent & event);
        void OnOpen(wxCommandEvent & event);
        void OnSave(wxCommandEvent & event);
        void OnPaneClose(wxAuiNotebookEvent & event);
        
        void CloseTab(size_t index);
        
        // life of these is tied to main window. So they are just pointers
        wxMenuBar     * m_menu;
        wxAuiManager    m_aui;
        wxAuiNotebook * m_docArea;
        
        // managed resources
        std::unique_ptr<MainWindow>     m_window;
        std::unique_ptr<IArtProvider>   m_artProvider;
        std::unique_ptr<MenuHandler>    m_menuHandler;
        std::unique_ptr<ToolbarHandler> m_tbarHandler;
        std::unordered_map<wxWindow *, CloseFn> m_closers;
        
        wxDECLARE_EVENT_TABLE();
    };
    
}
