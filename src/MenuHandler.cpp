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

using namespace fbide;

MenuHandler::MenuHandler(wxMenuBar * menu) : m_mbar(menu)
{
}


MenuHandler::~MenuHandler()
{
}


// Load Configuration
void MenuHandler::Load(Config & structure, wxMenu * parent)
{
    for (auto & node : structure.AsArray()) {
        auto & id   = node["id"].AsString();
        auto * menu = GetMenu(id);
        if (menu == nullptr) {
            menu = new wxMenu();
            m_map[id] = menu;
            if (node["show"] != false) {
                m_mbar->Append(menu, id);
            }
        }
        for (auto & item : node["items"].AsArray()) {
            if (item == "-") {
                menu->AppendSeparator();
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
        wxLogWarning ("Menu with id '%s' already registered", id);
        return;
    }
    
    m_map[id] = menu;
    
    if (show) {
        m_mbar->Append(menu, id);
    }
}


// Add a new item to the menu
void MenuHandler::AddItem(wxMenu * parent, const wxString & name)
{
    assert(parent != nullptr);
    assert(name != "");

    auto & ui  = GetUiMgr();
    auto & art = ui.GetArtProvider();
    
    auto item = new wxMenuItem(parent, wxID_ANY, name, name + " help", wxITEM_NORMAL);
    item->SetBitmap(art.GetIcon(name));
    parent->Append(item);
}


// Flag check items
void MenuHandler::OnEvent(wxCommandEvent & event)
{
}
