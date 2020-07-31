/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "pch.h"

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
    explicit ToolbarHandler(wxAuiManager* aui);
    ~ToolbarHandler() = default;

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
