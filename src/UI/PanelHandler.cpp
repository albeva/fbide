//
// Created by Albert on 7/27/2020.
//
#include "PanelHandler.hpp"
#include "App/Manager.hpp"
#include "UI/CmdManager.hpp"
using namespace fbide;

static auto ID_PanelsNotebook = ::wxNewId();

BEGIN_EVENT_TABLE(PanelHandler, wxEvtHandler)
    EVT_AUINOTEBOOK_PAGE_CLOSE(ID_PanelsNotebook, PanelHandler::OnPaneClose)
    EVT_COMMAND(wxID_ANY, CMD_CHECK, PanelHandler::OnCmdCheck)
END_EVENT_TABLE()

PanelHandler::PanelHandler(wxAuiManager *aui) : m_aui(aui) {
    // Panel area. Bottom area for compiler output, logs, etc tool panels
    m_panelArea = new wxAuiNotebook(
        m_aui->GetManagedWindow(),
        ID_PanelsNotebook,
        wxDefaultPosition,
        wxDefaultSize,
        wxAUI_NB_CLOSE_ON_ALL_TABS);

    auto info = wxAuiPaneInfo()
        .Center()
        .CaptionVisible(true)
        .Caption("Logs & Tools")
        .CloseButton(false)
        .PaneBorder(false)
        .Movable( false )
        .Dock()
        .Resizable()
        .FloatingSize(wxDefaultSize)
        .BottomDockable(true)
        .TopDockable(false)
        .LeftDockable(false)
        .RightDockable(false)
        .Floatable(false)
        .Row(0)
        .Position(0)
        .CentrePane();

    m_aui->AddPane(m_panelArea, info);
    m_aui->GetManagedWindow()->PushEventHandler(this);

}

PanelHandler::~PanelHandler() {
    m_aui->GetManagedWindow()->RemoveEventHandler(this);
}

void PanelHandler::OnPaneClose(wxAuiNotebookEvent &event) {
    wxLogMessage("Attempting to close log");
    event.Veto();
}

void PanelHandler::OnCmdCheck(wxCommandEvent &event) {
    event.Skip();
    wxLogMessage("Received OnCmdCheck");
}
