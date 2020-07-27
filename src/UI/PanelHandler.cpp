//
// Created by Albert on 7/27/2020.
//
#include "PanelHandler.hpp"
#include "App/Manager.hpp"
#include "UI/CmdManager.hpp"
#include "Config/ConfigManager.hpp"
using namespace fbide;

static auto ID_PanelsNotebook = ::wxNewId();

Panel::~Panel() = default;

BEGIN_EVENT_TABLE(PanelHandler, wxEvtHandler)
    EVT_AUINOTEBOOK_PAGE_CLOSE(ID_PanelsNotebook, PanelHandler::OnPaneClose)
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
        .CentrePane()
        .Hide();

    m_aui->AddPane(m_panelArea, info);
    m_aui->GetManagedWindow()->PushEventHandler(this);

    auto& cmdMgr = GetCmdMgr();
}

PanelHandler::~PanelHandler() {
    m_aui->GetManagedWindow()->RemoveEventHandler(this);
}

void PanelHandler::OnPaneClose(wxAuiNotebookEvent &event) {
    event.Veto();
    wxLogMessage("Attempting to close log");
}

void PanelHandler::OnCmdCheck(wxCommandEvent &event) {
    event.Skip();

    auto id = event.GetId();
    auto iter = m_idLookup.find(id);
    if (iter == m_idLookup.end()) {
        return;
    }

    auto& entry = iter->second;
    // wxLogMessage("Toggle panel '" + entry.name + '"');

    if (event.IsChecked() && entry.panel == nullptr) {
        if (entry.panel == nullptr) {
            entry.panel = entry.creator();
        }
        if (!entry.panel->Show()) {
            event.StopPropagation();
            GetCmdMgr().Check(id, false);
            if (entry.managed) {
                delete entry.panel;
            }
            entry.panel = nullptr;
        }
    }
}

void PanelHandler::Register(const wxString &name, int id, PanelHandler::PanelCreatorFn creator) {
    if (IsRegistered(name)) {
        wxLogWarning("Panel '" + name + "' is already registered with PanelHandler");
        return;
    }

    auto& cmdMgr = GetCmdMgr();
    if (cmdMgr.FindEntry(id) == nullptr) {
        cmdMgr.Register(name, CmdManager::Entry{id, CmdManager::Type::Check, false, true, nullptr});
    }

    auto insert = m_entries.emplace(name, Entry{name, id, Config::Empty, creator});
    m_idLookup.emplace(id, insert.first->second);
}
