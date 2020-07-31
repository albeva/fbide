//
//  ToolbarHandler.cpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "Document/TypeManager.hpp"
#include "ToolbarHandler.hpp"
#include "CmdManager.hpp"
#include "IArtProvider.hpp"
#include "UiManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"

using namespace fbide;

// toggle toolbars
namespace {
const int ID_ToggleToolbars = ::wxNewId(); // NOLINT
const int ToolBarStyle = wxAUI_TB_GRIPPER;
} // namespace


/**
 * Create toolbar handler
 */
ToolbarHandler::ToolbarHandler(wxAuiManager* aui)
: m_aui(aui), m_visible(true), m_visibleCnt(0) {
    auto *window = m_aui->GetManagedWindow();

    auto& cmd = GetCmdMgr();

    window->Bind(wxEVT_COMMAND_MENU_SELECTED, &ToolbarHandler::OnMenuSelected, this, wxID_ANY);
    window->Bind(wxEVT_SIZE, &ToolbarHandler::OnWindowResize, this, window->GetId());

    // toolbars menu
    m_menu = new wxMenu();
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
    auto *window = m_aui->GetManagedWindow();

    for (auto& node : structure.AsArray()) {
        auto& id = node["id"].AsString();
        bool show = node["show"] != false;
        bool add = false;

        auto *tbar = GetToolBar(id);
        if (tbar == nullptr) {
            tbar = new wxAuiToolBar{
                window,
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
    assert(!name.empty());

    auto& cmd = GetCmdMgr();
    auto& art = GetUiMgr().GetArtProvider();

    auto& entry = cmd.GetEntry(name);
    auto label = GetLang("Cmd." + name + ".name");
    auto help = GetLang("Cmd." + name + ".help");

    // type
    bool check = entry.type == CmdManager::Type::Check;

    auto *tool = tbar->AddTool(entry.id,
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
    if (GetToolBar(name) != nullptr) {
        wxLogWarning("Toolbar with id '%s' already exists", name); // NOLINT
        return;
    }
    m_tbars[name] = toolbar;

    // is toolbar really visible?
    bool isVisible = show && m_visible;

    // vars
    wxString label = GetLang("toolbar." + name);

    // Add to aui
    auto paneInfo = wxAuiPaneInfo()
        .Name(name)
        .Caption(label)
        .ToolbarPane()
        .Top()
        .Dockable(true)
        .Floatable(false)
        .Show(isVisible);
    m_aui->AddPane(toolbar, paneInfo);

    if (isVisible) {
        m_visibleCnt += 1;
    }
    m_visibleTbars[toolbar->GetId()] = show;

    // no menu... don't bother
    if (m_menu == nullptr) {
        return;
    }

    // Add menu item
    int menuId = ::wxNewId();
    wxMenuItem* item = m_menu->AppendCheckItem(menuId, label);
    item->Check(isVisible);

    // id bridge. Is this OK? each ID should be unique ... so it should be okay
    m_tbarMenuId[toolbar->GetId()] = menuId;
    m_tbarMenuId[menuId] = toolbar->GetId();
}

//-----------------------
// Handle menu commands
//-----------------------

// toolbars menu item clicked
void ToolbarHandler::OnMenuSelected(wxCommandEvent& event) {
    // allow other event handlers to catch this event
    event.Skip();

    // vars
    int id = event.GetId();

    // Toggle all toolbars
    if (id == ID_ToggleToolbars) {
        ShowToolbars(event.IsChecked());
        return;
    }

    // Toggle toolbar?
    auto iter = m_tbarMenuId.find(id);
    if (iter != m_tbarMenuId.end()) {
        bool show = event.IsChecked();
        int tbId = iter->second;
        ToggleToolbar(tbId, show);
        return;
    }
}

void ToolbarHandler::ShowToolbars(bool show) {
    if (m_visible == show) {
        return;
    }
    m_visible = show;

    for (const auto& iter : m_tbars) {
        auto *tbar = iter.second;
        if (m_visibleTbars[tbar->GetId()]) {
            m_aui->GetPane(tbar).Show(show);
            m_visibleCnt += show ? 1 : -1;

            auto idIter = m_tbarMenuId.find(tbar->GetId());
            if (idIter == m_tbarMenuId.end()) {
                continue;
            }

            if (m_menu != nullptr) {
                auto *item = m_menu->FindItem(idIter->second);
                if (item != nullptr) {
                    item->Check(show);
                }
            }
        }
    }
    m_aui->Update();

    // check
    GetCmdMgr().Check(ID_ToggleToolbars, m_visibleCnt != 0);
}

void ToolbarHandler::ToggleToolbar(int id, bool show) {
    if (!m_visible && show) {
        for (auto& b : m_visibleTbars) {
            b.second = false;
        }
        m_visibleCnt = 0;
    }

    // toggle toolbar visibility
    for (const auto& tbar : m_tbars) {
        auto& pane = m_aui->GetPane(tbar.second);
        if (pane.window->GetId() != id) {
            continue;
        }
        pane.Show(show);
        m_visibleCnt += show ? 1 : -1;
        m_visibleTbars[id] = show;
        m_aui->Update();
    }

    // Set toggle button state
    m_visible = m_visibleCnt != 0;
    GetCmdMgr().Check(ID_ToggleToolbars, m_visible);
    GetCmdMgr().Enable(ID_ToggleToolbars, m_visible);
}

/**
 * Show or hide overflow buttons on toolbars when toolbar size is changed
 * due to window reize
 */
void ToolbarHandler::OnWindowResize(wxSizeEvent& event) {
    event.Skip();
    auto *window = m_aui->GetManagedWindow();
    auto width = window->GetClientSize().GetWidth() - 2;
    for(const auto& pair: m_tbars) {
        auto *tbar = pair.second;
        auto right = tbar->GetRect().GetRight();
        auto overflow = right >= width;

        if (overflow != tbar->GetOverflowVisible()) {
            tbar->SetOverflowVisible(overflow);
            tbar->Realize();
        }
    }
}
