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
