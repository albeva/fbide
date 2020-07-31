//
//  MenuHandler.hpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class Config;

/**
 * Handle menus
 */
class MenuHandler final {
    NON_COPYABLE(MenuHandler)
public:
    explicit MenuHandler(wxMenuBar* menu);
    ~MenuHandler() = default;

    // Load Configuration
    void Load(Config& node, wxMenu* parent = nullptr);

    // Get menu by ID
    wxMenu* GetMenu(const wxString& id);

    // Add new menu
    void Add(const wxString& id, wxMenu* menu, bool show = false);

    // Add a new item to the menu
    void AddItem(wxMenu* parent, const wxString& id);

private:
    StringMap<wxMenu*> m_map;
    wxMenuBar* m_mbar;
};

} // namespace fbide
