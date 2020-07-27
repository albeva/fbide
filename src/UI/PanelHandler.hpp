//
// Created by Albert on 7/27/2020.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

/**
 * Manage panel area.
 * Register panel providers. Similar to Toolbar and menu system
 *
 * Each registered panel
 * - has name that can be looked up from lang
 * - icon that can be fetched from ArtProvider
 * - options:
 *   * closable
 * - panel UI can still be loaded, but may be detached / reattached
 * - panels can be toggled from toolbar / menu
 */
class PanelHandler final: NonCopyable, public wxEvtHandler {
public:
    PanelHandler(wxAuiManager* aui);
    ~PanelHandler();

    inline wxAuiNotebook* GetPanelArea() { return m_panelArea; }

private:
    void OnPaneClose(wxAuiNotebookEvent& event);
    void OnCmdCheck(wxCommandEvent& event);

    wxAuiNotebook* m_panelArea = nullptr;
    wxAuiManager* m_aui = nullptr;

    wxDECLARE_EVENT_TABLE();
};

}
