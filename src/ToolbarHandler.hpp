//
//  ToolbarHandler.hpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    /**
     * Handle toolbars
     */
    class ToolbarHandler : NonCopyable
    {
    public:
        
        /**
         * Create toolbar handler
         */
        ToolbarHandler(wxAuiManager * aui);
        
        /**
         * Load toolbars from configuration
         */
        void Load(Config & config);
        
        /**
         * Get toolbar for given id. nullptr if not found
         */
        wxAuiToolBar * GetToolBar(const wxString & id);
        
        /**
         * Add toolbar
         */
        void AddToolBar (const wxString & name, wxAuiToolBar * toolbar, bool show = true);
        
        /**
         * Add toolbar item
         */
        void AddItem(wxAuiToolBar * tbar, const wxString & name);
        
        /**
         * Handle events
         */
        void OnEvent(wxCommandEvent & event);
        
        
    private:
        
        wxAuiManager * m_aui;
        wxMenu       * m_menu;          // toolbars menu
        wxWindow     * m_window;        // owning window
        bool           m_visible{true}; // toolbars visible
        std::unordered_map<wxString, wxAuiToolBar*> m_tbars;
    };
    
}
