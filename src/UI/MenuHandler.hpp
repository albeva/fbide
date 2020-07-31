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
