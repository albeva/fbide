//
//  ToolbarHandler.hpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class Config;

/**
 * Handle toolbars
 */
class ToolbarHandler final {
    NON_COPYABLE(ToolbarHandler)
public:
    /**
     * Create toolbar handler
     */
    ToolbarHandler(wxAuiManager* aui);

    /**
     * Load toolbars from configuration
     */
    void Load(Config& config);

private:

    wxAuiToolBar* GetToolBar(const wxString& id);
    void AddToolBar(const wxString& name, wxAuiToolBar* toolbar, bool show = true);
    void AddItem(wxAuiToolBar* tbar, const wxString& name);

    void OnMenuSelected(wxCommandEvent& event);
    void ShowToolbars(bool show);
    void ToggleToolbar(int id, bool show);

    void OnWindowResize(wxSizeEvent& event);

    wxAuiManager* m_aui;
    wxMenu* m_menu;     // toolbars menu
    bool m_visible;     // toolbars visible
    int m_visibleCnt;   // visible toolbar count
    StringMap<wxAuiToolBar*> m_tbars;
    std::unordered_map<int, bool> m_visibleTbars;
    std::unordered_map<int, int> m_tbarMenuId;
};

} // namespace fbide
