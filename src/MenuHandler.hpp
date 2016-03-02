//
//  MenuHandler.hpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    class Config;
    
    /**
     * Handle menus
     */
    class MenuHandler : NonCopyable
    {
    public:

        MenuHandler(wxMenuBar * menu);
        virtual ~MenuHandler();
        
        // Load Configuration
        void Load (Config & node, wxMenu * parent = nullptr);
        
        // Get menu by ID
        wxMenu * GetMenu (const wxString & id);
        
        // Add new menu
        void Add (const wxString & id, wxMenu * menu, bool show = false);
        
        // Add a new item to the menu
        void AddItem (wxMenu * parent, const wxString & id);
        
        // Flag check items
        void OnEvent(wxCommandEvent & event);
        
    private:
            std::unordered_map<wxString, wxMenu*> m_map;
            wxMenuBar * m_mbar;
    };
    
}
