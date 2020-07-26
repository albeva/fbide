//
//  ToolbarHandler.cpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "ToolbarHandler.hpp"
#include "CmdManager.hpp"
#include "IArtProvider.hpp"
#include "UiManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"

using namespace fbide;

// toggle toolbars
namespace {
const int ID_ToggleToolbars = ::wxNewId();
const int ToolBarStyle = (wxAUI_TB_GRIPPER | wxAUI_TB_OVERFLOW);
} // namespace


/**
 * Create toolbar handler
 */
ToolbarHandler::ToolbarHandler(wxAuiManager* aui)
: m_aui(aui), m_visible(true), m_visibleCnt(0) {
    m_window = aui->GetManagedWindow();

    auto& cmd = GetCmdMgr();

    // bind events
    cmd.Bind(CMD_CHECK, &ToolbarHandler::OnEvent, this, wxID_ANY);
    cmd.Bind(CMD_ENABLE, &ToolbarHandler::OnEvent, this, wxID_ANY);

    //
    m_aui->Bind(wxEVT_AUI_PANE_CLOSE, &ToolbarHandler::OnPaneClose, this, wxID_ANY);
    m_window->Bind(wxEVT_COMMAND_MENU_SELECTED, &ToolbarHandler::OnCommandEvent, this, wxID_ANY);

    // toolbars menu
    m_menu = new wxMenu;
    cmd.Register("toolbars", { ::wxNewId(), CmdManager::Type::Menu, true, true, m_menu });

    // toggle toolbars
    cmd.Register("toolbars.toggle", { ID_ToggleToolbars, CmdManager::Type::Check, m_visible });
}


/**
 * Load toolbars from configuration
 */
void ToolbarHandler::Load(Config& structure) {
    auto& ui = GetUiMgr();
    auto& art = ui.GetArtProvider();

    for (auto& node : structure.AsArray()) {
        auto& id = node["id"].AsString();
        bool show = node["show"] != false;
        bool add = false;

        auto tbar = GetToolBar(id);
        if (tbar == nullptr) {
            tbar = new wxAuiToolBar{
                m_window,
                wxID_ANY,
                wxDefaultPosition,
                wxDefaultSize,
                ToolBarStyle
            };
            tbar->SetToolBitmapSize(art.GetIconSize());
            add = true;
        }

        // toolbar items
        for (auto& child : node["items"].AsArray()) {
            if (child == "-") {
                tbar->AddSeparator();
            } else {
                AddItem(tbar, child.AsString());
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
void ToolbarHandler::AddItem(wxAuiToolBar* tbar, const wxString& name) {
    assert(tbar != nullptr);
    assert(name != "");

    auto& cmd = GetCmdMgr();
    auto& art = GetUiMgr().GetArtProvider();

    auto& entry = cmd.GetEntry(name);
    auto label = GetLang("Cmd." + name + ".name");
    auto help = GetLang("Cmd." + name + ".help");

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
wxAuiToolBar* ToolbarHandler::GetToolBar(const wxString& id) {
    auto iter = m_tbars.find(id);
    return iter == m_tbars.end() ? nullptr : iter->second;
}


/**
 * Add toolbar
 */
void ToolbarHandler::AddToolBar(const wxString& name, wxAuiToolBar* toolbar, bool show) {
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
    m_aui->AddPane(toolbar, wxAuiPaneInfo().Name(name).Caption(label).ToolbarPane().Top().Dockable(true).Floatable(false).Show(isVisible));

    if (isVisible)
        m_visibleCnt += 1;
    m_visibleTbars[toolbar->GetId()] = show;

    // no menu... don't bother
    if (m_menu == nullptr)
        return;

    // Add menu item
    int menuId = ::wxNewId();
    wxMenuItem* item = m_menu->AppendCheckItem(menuId, label);
    item->Check(isVisible);

    // id bridge. Is this OK? each ID should be unique ... so it should be okay
    m_idBridge[toolbar->GetId()] = menuId;
    m_idBridge[menuId] = toolbar->GetId();
}


/**
 * Show (or hide) all toolbars
 */
void ToolbarHandler::ShowToolbars(bool show) {
    if (m_visible == show)
        return;
    m_visible = show;

    for (auto iter : m_tbars) {
        auto tbar = iter.second;
        if (m_visibleTbars[tbar->GetId()]) {
            m_aui->GetPane(tbar).Show(show);
            m_visibleCnt += show ? 1 : -1;

            auto idIter = m_idBridge.find(tbar->GetId());
            if (idIter == m_idBridge.end())
                continue;

            if (m_menu) {
                auto item = m_menu->FindItem(idIter->second);
                if (item) {
                    item->Check(show);
                }
            }
        }
    }
    m_aui->Update();

    // check
    GetCmdMgr().Check(ID_ToggleToolbars, m_visibleCnt != 0);
}


/**
 * Handle events
 */
void ToolbarHandler::OnEvent(wxCommandEvent& event) {
    // allow others to catch the event
    event.Skip();

    // update the toolbars
    wxAuiPaneInfoArray& panes = m_aui->GetAllPanes();
    for (size_t iter = 0; iter < panes.Count(); iter++) {
        wxAuiPaneInfo& pane = panes[iter];

        if (!pane.IsToolbar())
            continue;
        auto tbar = dynamic_cast<wxAuiToolBar*>(pane.window);
        if (tbar == nullptr)
            continue;

        if (event.GetEventType() == CMD_CHECK) {
            tbar->ToggleTool(event.GetId(), event.IsChecked());
        } else if (event.GetEventType() == CMD_ENABLE) {
            tbar->EnableTool(event.GetId(), event.IsChecked());
        }
    }

    m_aui->Update();
}


// toolbars menu item clicked
void ToolbarHandler::OnCommandEvent(wxCommandEvent& event) {
    // allow other event handlers to catch this event
    event.Skip();

    // vars
    int id = event.GetId();

    // Toggle all toolbars
    if (id == ID_ToggleToolbars) {
        ShowToolbars(event.IsChecked());
        return;
    }

    // find the id. Skip otherwise
    auto iter = m_idBridge.find(id);
    if (iter == m_idBridge.end())
        return;

    bool show = event.IsChecked();
    int tbId = iter->second;

    // show only given toolbar (since all others were hidden)
    if (!m_visible && show) {
        for (auto& b : m_visibleTbars) {
            b.second = false;
        }
        m_visibleCnt = 0;
    }

    // toggle toolbar visibility
    for (auto tbar : m_tbars) {
        auto& pane = m_aui->GetPane(tbar.second);
        if (pane.window->GetId() == tbId) {
            pane.Show(show);
            m_visibleCnt += show ? 1 : -1;
            m_visibleTbars[tbId] = show;
            m_aui->Update();
            break;
        }
    }

    // Set toggle button state
    if (m_visibleCnt) {
        m_visible = true;
    }
    GetCmdMgr().Check(ID_ToggleToolbars, m_visibleCnt != 0);
    GetCmdMgr().Enable(ID_ToggleToolbars, m_visibleCnt != 0);
}


// Close toolbar?
void ToolbarHandler::OnPaneClose(wxAuiManagerEvent& event) {
    // allow other event handlers to catch this event by defalt
    event.Skip();

    // not a toolbar pane ?
    if (!event.GetPane()->IsToolbar())
        return;

    // find id mapping
    int tbId = event.GetPane()->window->GetId();
    auto iter = m_idBridge.find(tbId);
    if (iter == m_idBridge.end())
        return;

    // don't skip event
    event.Skip(false);

    // update the menu
    if (m_menu) {
        wxMenuItem* item = m_menu->FindItem(iter->second);
        if (item) {
            item->Check(false);
        }
    }

    m_visibleCnt--;
    m_visibleTbars[tbId] = false;

    // disable
    GetCmdMgr().Check(ID_ToggleToolbars, m_visibleCnt != 0);
    GetCmdMgr().Enable(ID_ToggleToolbars, m_visibleCnt != 0);
}
