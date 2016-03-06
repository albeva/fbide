//
//  ToolbarHandler.cpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "ToolbarHandler.hpp"
#include "CmdManager.hpp"
#include "UiManager.hpp"
#include "IArtProvider.hpp"

using namespace fbide;

// toggle toolbars
namespace {
    const int ID_ToggleToolbars = ::wxNewId();
    const int ToolBarStyle = wxAUI_TB_GRIPPER
                           | wxAUI_TB_HORZ_LAYOUT
                           | wxAUI_TB_OVERFLOW
                           ;
}


/**
 * Create toolbar handler
 */
ToolbarHandler::ToolbarHandler(wxAuiManager * aui) : m_aui(aui)
{
    m_window = aui->GetManagedWindow();
    
    auto & cmd = GetCmdMgr();

    // bind events
    cmd.Bind(CMD_CHECK,  &ToolbarHandler::OnEvent, this, wxID_ANY);
    cmd.Bind(CMD_ENABLE, &ToolbarHandler::OnEvent, this, wxID_ANY);

    // toolbars menu
    m_menu = new wxMenu;
    cmd.Register("menu.toolbars", {
        ::wxNewId(),
        CmdManager::Type::Menu,
        true,
        true,
        m_menu
    });
    
    // toggle toolbars
    cmd.Register("toolbars.togglef", {
        ID_ToggleToolbars,
        CmdManager::Type::Check,
        true,
        true
    });
}


/**
 * Load toolbars from configuration
 */
void ToolbarHandler::Load(Config & structure)
{
    auto & ui  = GetUiMgr();
    auto & art = ui.GetArtProvider();
    
    for (auto & node : structure.AsArray()) {
        auto & id = node["id"].AsString();
        bool show = node["show"] != false;
        bool add = false;
        
        auto tbar = GetToolBar(id);
        if (tbar == nullptr) {
            tbar = new wxAuiToolBar{
                m_window,
                wxID_ANY,
                wxDefaultPosition,
                wxDefaultSize,
                ToolBarStyle};
            tbar->SetToolBitmapSize(art.GetSize());
            add = true;
        }
        
        // toolbar items
        for (auto & child : node["items"].AsArray()) {
            if (child == "-") {
                tbar->AddSeparator();
            } else {
                AddItem(tbar, child);
            }
        }
        
        if (add) {
            AddToolBar(id, tbar, show);
        }
        
        tbar->Realize();
    }
}


/**
 * Add toolbar item
 */
void ToolbarHandler::AddItem(wxAuiToolBar * tbar, const wxString & name)
{
    assert(tbar != nullptr);
    assert(name != "");
    
    auto & cmd = GetCmdMgr();
    auto & art = GetUiMgr().GetArtProvider();
    
    auto &entry = cmd.GetEntry(name);
    auto label  = GetLang(name + ".name");
    auto help   = GetLang(name + ".help");
    
    // type
    bool check = entry.type == CmdManager::Type::Check;
    
    auto tool = tbar->AddTool(entry.id,
                              label,
                              art.GetIcon(name),
                              help,
                              check ? wxITEM_CHECK : wxITEM_NORMAL);
    tool->SetLongHelp(help);
    
    if (check) {
        tbar->ToggleTool(entry.id, entry.checked);
    }
}


/**
 * Find toolbar
 */
wxAuiToolBar * ToolbarHandler::GetToolBar(const wxString & id)
{
    auto iter = m_tbars.find(id);
    return iter == m_tbars.end() ? nullptr : iter->second;
}


/**
 * Add toolbar
 */
void ToolbarHandler::AddToolBar(const wxString & name, wxAuiToolBar * toolbar, bool show)
{
    if (GetToolBar(name)) {
        wxLogWarning("Toolbar with id '%s' already exists", name);
        return;
    }
    m_tbars[name] = toolbar;
    
    // is toolbar really visible?
    bool isVisible = show && m_visible;
    
    // vars
    wxString label = GetLang("toolbar." + name);
    
    // Add to aui
    m_aui->AddPane(toolbar, wxAuiPaneInfo()
                   .Name(name)
                   .Caption(label)
                   .ToolbarPane().Top().Dockable(true).Show(isVisible));
    
//    if (isVisible) m_visibleCnt += 1;
//    m_visibleMap[toolbar->GetId()] = show;
    
    // no menu... don't bother
    if (m_menu == nullptr) return;
    
    // Add menu item
    int menuId = ::wxNewId();
    wxMenuItem * item = m_menu->AppendCheckItem(menuId, label);
    item->Check(isVisible);
    
    // id bridge. Is this OK? each ID should be unique ... so it should be okay
//    m_idbridge[toolbar->GetId()] = menuId;
//    m_idbridge[menuId] = toolbar->GetId();
}


/**
 * Handle events
 */
void ToolbarHandler::OnEvent(wxCommandEvent & event)
{
    // allow others to catch the event
    event.Skip();
    
    // update the toolbars
    wxAuiPaneInfoArray & panes = m_aui->GetAllPanes();
    for (size_t iter = 0; iter < panes.Count(); iter++)
    {
        wxAuiPaneInfo & pane = panes[iter];
        if (!pane.IsToolbar()) continue;
        auto tbar = dynamic_cast<wxAuiToolBar*>(pane.window);
        if (tbar == nullptr) continue;
        
        if (event.GetEventType() == CMD_CHECK) {
            tbar->ToggleTool(event.GetId(), event.IsChecked());
        } else if (event.GetEventType() == CMD_ENABLE) {
            tbar->EnableTool(event.GetId(), event.IsChecked());
        }
    }
    
    m_aui->Update();
}
