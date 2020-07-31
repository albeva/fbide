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
#include "MenuHandler.hpp"
#include "CmdManager.hpp"
#include "IArtProvider.hpp"
#include "UiManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"

using namespace fbide;

static const wxString ID        = "id";             // NOLINT
static const wxString SHOW      = "show";           // NOLINT
static const wxString ITEMS     = "items";          // NOLINT
static const wxString DASH      = "-";              // NOLINT
static const wxString MENU_ICON = "App.MenuIcons";  // NOLINT


// listen for events
MenuHandler::MenuHandler(wxMenuBar* menu) : m_mbar(menu) {
}

// Load Configuration
void MenuHandler::Load(Config& structure, wxMenu* parent) {
    auto& cmd = GetCmdMgr();

    for (auto& node : structure.AsArray()) { // NOLINT
        auto& id = node[ID].AsString();

        auto* menu = GetMenu(id);
        if (menu == nullptr) {
            const auto *entry = cmd.FindEntry(id);

            // check if menu is in cmd manager
            if (entry != nullptr && entry->type == CmdManager::Type::Menu) {
                menu = dynamic_cast<wxMenu*>(entry->object);
            }

            if (menu == nullptr) {
                menu = new wxMenu ();
            }

            // show this item?
            bool show = node[SHOW] != false && menu->GetParent() == nullptr && menu->GetMenuBar() == nullptr;

            // add it
            Add(id, menu, parent == nullptr && show);
            if ((parent != nullptr) && show) {
                parent->AppendSubMenu(menu, GetLang("menu." + id, id));
            }
        }

        for (auto& item : node[ITEMS].AsArray()) {
            if (item == "-") {
                menu->AppendSeparator();
            } else if (item.IsArray()) {
                Load(item, menu);
            } else if (item.IsMap()) {
                auto arr = Config(Config::Array{item});
                Load(arr, menu);
            } else {
                AddItem(menu, item.AsString());
            }
        }
    }
}

// Get menu by ID
wxMenu* MenuHandler::MenuHandler::GetMenu(const wxString& id) {
    auto iter = m_map.find(id);
    return iter == m_map.end() ? nullptr : iter->second;
}

// Add new menu
void MenuHandler::Add(const wxString& id, wxMenu* menu, bool show) {
    assert(!id.empty());
    assert(menu != nullptr);

    if (GetMenu(id) != nullptr) {
        wxLogWarning("Menu with id '%s' already registered", id); // NOLINT
        return;
    }

    m_map[id] = menu;

    if (show) {
        m_mbar->Append(menu, GetLang("menu." + id, id));
    }
}

// Add a new item to the menu
void MenuHandler::AddItem(wxMenu* parent, const wxString& id) {
    assert(parent != nullptr);
    assert(!id.empty());

    auto& ui = GetUiMgr();
    auto& art = ui.GetArtProvider();
    auto& cmd = GetCmdMgr();
    auto& entry = cmd.GetEntry(id);
    auto& cfg = GetConfig();
    const auto& name = GetLang("Cmd." + id + ".name", id);
    const auto& help = GetLang("Cmd." + id + ".help", id);

    if (entry.type == CmdManager::Type::Menu) {
        auto *item = parent->AppendSubMenu(dynamic_cast<wxMenu*>(entry.object), name);
        if (cfg.Get(MENU_ICON, true)) {
            item->SetBitmap(art.GetIcon(id));
        }
    } else {
        bool check = entry.type == CmdManager::Type::Check;

        auto *item = new wxMenuItem{
            parent,
            entry.id,
            name,
            help,
            check ? wxITEM_CHECK : wxITEM_NORMAL
        };

        parent->Append(item);

        if (check) {
            item->Check(entry.checked);
        } else if (cfg.Get(MENU_ICON, true)) {
            item->SetBitmap(art.GetIcon(id));
        }
    }
}
