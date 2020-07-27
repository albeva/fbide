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

    /**
     * Get toolbar for given id. nullptr if not found
     */
    wxAuiToolBar* GetToolBar(const wxString& id);

    /**
     * Add toolbar
     */
    void AddToolBar(const wxString& name, wxAuiToolBar* toolbar, bool show = true);

    /**
     * Add toolbar item
     */
    void AddItem(wxAuiToolBar* tbar, const wxString& name);

    /**
     * Show (or hide) all toolbars
     */
    void ShowToolbars(bool show);

    /**
     * Handle events
     */
    void OnEvent(wxCommandEvent& event);

    /**
     * Handle toolbar pane close event
     */
    void OnPaneClose(wxAuiManagerEvent& event);

    /**
     * Listen for menu command (toggle toolbar(s))
     */
    void OnMenuSelected(wxCommandEvent& event);


private:
    wxAuiManager* m_aui;
    wxMenu* m_menu;     // toolbars menu
    wxWindow* m_window; // owning window
    bool m_visible;     // toolbars visible
    int m_visibleCnt;   // visible toolbar count
    StringMap<wxAuiToolBar*> m_tbars;
    std::unordered_map<int, bool> m_visibleTbars;
    std::unordered_map<int, int> m_idBridge;
};

} // namespace fbide
