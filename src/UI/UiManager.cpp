//
//  UiManager.cpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "UiManager.hpp"
#include "Config/ConfigManager.hpp"

#include "CmdManager.hpp"
#include "Document/Document.hpp"
#include "Document/TypeManager.hpp"
#include "MenuHandler.hpp"
#include "StandardArtProvider.hpp"
#include "ToolbarHandler.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"
#include "PanelHandler.hpp"
#include "App/App.hpp"

using namespace fbide;

namespace {
const int ID_AppWindow = ::wxNewId();       // NOLINT
const int ID_FullScreen = ::wxNewId();      // NOLINT
const int ID_DocNotebook = ::wxNewId();     // NOLINT
const int ID_PanelNotebook = ::wxNewId();   // NOLINT
} // namespace

// event dispatching
wxBEGIN_EVENT_TABLE(UiManager, wxEvtHandler)    // NOLINT
    EVT_MENU(wxID_ANY, UiManager::HandleMenuEvents)
    EVT_AUINOTEBOOK_PAGE_CLOSE(ID_DocNotebook, UiManager::OnPaneClose)
    EVT_UPDATE_UI(wxID_ANY, UiManager::OnUpdateUI)
    EVT_CLOSE(UiManager::OnWindowClose)
wxEND_EVENT_TABLE()

// Instantiate classes, but specific
// loading (especially dependant on configuration)
// is deferred to Load method
UiManager::UiManager() {
    // the frame
    m_window = std::make_unique<wxFrame>(nullptr, ID_AppWindow, "fbide", wxDefaultPosition, wxSize(640, 480)); // NOLINT
    m_window->PushEventHandler(this);
    m_window->EnableFullScreenView(true);
    wxGetApp().SetTopWindow(m_window.get());
    m_aui.SetManagedWindow(m_window.get());

    // Load default the art provider
    m_artProvider = std::make_unique<StandardArtProvider>();

    // the menu
    m_menu = new wxMenuBar();
    m_window->SetMenuBar(m_menu);

    // menu handler
    m_menuHandler = std::make_unique<MenuHandler>(m_menu);

    // aui
    m_aui.SetFlags(wxAUI_MGR_LIVE_RESIZE | wxAUI_MGR_DEFAULT); // NOLINT
    m_aui.SetManagedWindow(m_window.get());
    m_aui.Update();

    // toolbar handler
    m_tbarHandler = std::make_unique<ToolbarHandler>(&m_aui);

    // main doc area
    m_docArea = new wxAuiNotebook(m_window.get(),
        ID_DocNotebook,
        wxDefaultPosition,
        wxDefaultSize,
        wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_WINDOWLIST_BUTTON | wxAUI_NB_CLOSE_ON_ALL_TABS); // NOLINT
    m_aui.AddPane(m_docArea, wxAuiPaneInfo().Name("DocArea").CenterPane().PaneBorder(false));

    // panels
    m_panelHandler = std::make_unique<PanelHandler>(&m_aui);

    // toggle full screen
    GetCmdMgr().Register("fullscreen", { ID_FullScreen, CmdManager::Type::Check, false });
}

// shut down the UI. Clean things
// up in proper order
UiManager::~UiManager() {
    m_window->RemoveEventHandler(this);
    m_window->Destroy();
}

/**
 * Shotdown the manager
 */
void UiManager::Unload() {
    while (m_docArea->GetPageCount() != 0) {
        CloseTab(0);
    }
}

// Load the UI
void UiManager::Load() {
    auto& layoutCfg = GetConfig("Layout");
    m_menuHandler->Load(layoutCfg["Menus"]);
    m_tbarHandler->Load(layoutCfg["Toolbars"]);
    m_aui.Update();
}

void UiManager::SetArtProvider(IArtProvider* artProvider) {
    m_artProvider = std::unique_ptr<IArtProvider>{ artProvider };
}

void UiManager::HandleMenuEvents(wxCommandEvent& event) {
    auto id = event.GetId();


    // let CmdMgr check the status (if this is a registered check)
    GetCmdMgr().Check(event.GetId(), event.IsChecked());

    if (id == ID_FullScreen) {
        m_window->ShowFullScreen(event.IsChecked());
    } else if (id == wxID_EXIT) {
        App::ExitFBIde();
    } else {
        event.Skip();
    }
}

void UiManager::OnUpdateUI(wxUpdateUIEvent &event) {
    auto id = event.GetId();
    const auto* entry = GetCmdMgr().FindEntry(id);

    if (entry == nullptr) {
        return;
    }

    if (entry->type == CmdManager::Type::Check) {
        event.Check(entry->checked);
    }
    event.Enable(entry->enabled);
}

//// Handle window closing

void UiManager::OnWindowClose(wxCloseEvent& close) {
    close.Veto();
    App::ExitFBIde();
}

// TODO: Move tab handling out of UIManager

void UiManager::OnPaneClose(wxAuiNotebookEvent& event) {
    event.Veto();
    CloseTab(event.GetSelection());
}

void UiManager::CloseTab(size_t index) {
    m_docArea->DeletePage(index);
}
