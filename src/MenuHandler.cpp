//
//  MenuHandler.cpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "MenuHandler.hpp"
#include "UiManager.hpp"
#include "IArtProvider.hpp"
#include "CmdManager.hpp"

using namespace fbide;


// listen for events
MenuHandler::MenuHandler(wxMenuBar * menu) : m_mbar(menu)
{
    auto & cmd = GetCmdMgr();
    cmd.Bind(CMD_CHECK,  &MenuHandler::OnEvent, this, wxID_ANY);
    cmd.Bind(CMD_ENABLE, &MenuHandler::OnEvent, this, wxID_ANY);
}


// Load Configuration
void MenuHandler::Load(Config & structure, wxMenu * parent)
{
    auto & cmd = GetCmdMgr();
    
    for (auto & node : structure.AsArray()) {
        auto & id   = node["id"].AsString();
        
        auto * menu = GetMenu(id);
        if (menu == nullptr) {
            auto entry = cmd.FindEntry(id);
            
            // check if menu is in cmd manager
            if (entry != nullptr &&
                entry->type == CmdManager::Type::Menu &&
                (menu = dynamic_cast<wxMenu *>(entry->object)));
            else menu = new wxMenu;
            
            // show this item?
            bool show = node["show"] != false &&
                 menu->GetParent() == nullptr &&
                 menu->GetMenuBar() == nullptr;
            
            // add it
            Add(id, menu, parent == nullptr && show);
            if (parent && show) {
                parent->AppendSubMenu(menu, GetLang("menu." + id, id));
            }
        }
        
        for (auto & item : node["items"].AsArray()) {
            if (item == "-") {
                menu->AppendSeparator();
            } else if (item.IsArray()) {
                Load(item, menu);
            } else if (item.IsMap()) {
                auto arr = Config{Config::Array{item}};
                Load(arr, menu);
            } else {
                AddItem(menu, item);
            }
        }
    }
}


// Get menu by ID
wxMenu * MenuHandler::MenuHandler::GetMenu(const wxString & id)
{
    auto iter = m_map.find(id);
    return iter == m_map.end() ? nullptr : iter->second;
}


// Add new menu
void MenuHandler::Add(const wxString & id, wxMenu * menu, bool show)
{
    assert(id != "");
    assert(menu != nullptr);
    
    if (GetMenu(id) != nullptr) {
        wxLogWarning("Menu with id '%s' already registered", id);
        return;
    }
    
    m_map[id] = menu;
    
    if (show) {
        m_mbar->Append(menu, GetLang("menu." + id, id));
    }
}


// Add a new item to the menu
void MenuHandler::AddItem(wxMenu * parent, const wxString & id)
{
    assert(parent != nullptr);
    assert(id != "");

    auto & ui    = GetUiMgr();
    auto & art   = ui.GetArtProvider();
    auto & cmd   = GetCmdMgr();
    auto & entry = cmd.GetEntry(id);
    auto & name  = GetLang(id + ".name", id);
    auto & help  = GetLang(id + ".help", id);
    
    if (entry.type == CmdManager::Type::Menu) {
        auto item = parent->AppendSubMenu(dynamic_cast<wxMenu*>(entry.object), name);
        item->SetBitmap(art.GetIcon(id));
    } else {
        bool check = entry.type == CmdManager::Type::Check;
        
        auto item = new wxMenuItem{
            parent,
            entry.id,
            name,
            help,
            check ? wxITEM_CHECK : wxITEM_NORMAL
        };
        
        parent->Append(item);

		if (check) {
			item->Check(entry.checked);
		} else {
			item->SetBitmap(art.GetIcon(id));
		}

    }
}


// Flag check items
void MenuHandler::OnEvent(wxCommandEvent & event)
{
    auto item = m_mbar->FindItem(event.GetId());
    if (item == nullptr) return;
    
    if (event.GetEventType() == CMD_CHECK) {
        item->Check(event.IsChecked());
    } else if (event.GetEventType() == CMD_ENABLE) {
        item->Enable(event.GetInt());
    }
}
