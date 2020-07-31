//
//  UiManager.hpp
//  fbide
//
//  Created by Albert on 05/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "pch.h"

namespace fbide {

class IArtProvider;
class MenuHandler;
class ToolbarHandler;
class PanelHandler;

/**
 * Manage fbide UI.
 * app frame, menus, toolbars and panels
 */
class UiManager final: public wxEvtHandler {
    NON_COPYABLE(UiManager)
public:

    UiManager();
    ~UiManager() final;

    void Load();
    void Unload();

    [[nodiscard]] inline wxFrame* GetWindow() { return m_window.get(); }
    [[nodiscard]] inline wxAuiNotebook* GetDocArea() { return m_docArea; }
    [[nodiscard]] inline PanelHandler* GetPanelHandler() { return m_panelHandler.get(); }

    void SetArtProvider(IArtProvider* artProvider);
    [[nodiscard]] inline IArtProvider& GetArtProvider() const { return *m_artProvider; }

private:
    void HandleMenuEvents(wxCommandEvent& event);
    void OnPaneClose(wxAuiNotebookEvent& event);
    void OnUpdateUI(wxUpdateUIEvent & event);
    void OnWindowClose(wxCloseEvent &close);

    void CloseTab(size_t index);

    // life of these is tied to main window. So they are just pointers
    wxMenuBar* m_menu;
    wxAuiManager m_aui;
    wxAuiNotebook* m_docArea;

    // managed resources
    std::unique_ptr<wxFrame> m_window;
    std::unique_ptr<IArtProvider> m_artProvider;
    std::unique_ptr<MenuHandler> m_menuHandler;
    std::unique_ptr<ToolbarHandler> m_tbarHandler;
    std::unique_ptr<PanelHandler> m_panelHandler;

    wxDECLARE_EVENT_TABLE(); // NOLINT
};

} // namespace fbide
