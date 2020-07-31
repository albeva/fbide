//
// Created by Albert on 7/27/2020.
//
#include "PanelHandler.hpp"
#include "App/Manager.hpp"
#include "UI/CmdManager.hpp"
#include "Config/ConfigManager.hpp"
using namespace fbide;

static auto ID_PanelsNotebook = ::wxNewId(); // NOLINT

Panel::~Panel() = default;

BEGIN_EVENT_TABLE(PanelHandler, wxEvtHandler) // NOLINT
    EVT_AUINOTEBOOK_PAGE_CLOSE(ID_PanelsNotebook,  PanelHandler::OnPaneWillClose)
    EVT_MENU(wxID_ANY, PanelHandler::HandleMenuEvents)
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
        .Hide()
        ;

    m_aui->AddPane(m_panelArea, info);
    m_aui->GetManagedWindow()->PushEventHandler(this);
}

PanelHandler::~PanelHandler() {
    m_aui->GetManagedWindow()->RemoveEventHandler(this);
}

void PanelHandler::OnPaneWillClose(wxAuiNotebookEvent &event) {
    event.Veto();

    int index = event.GetSelection();
    auto *window = m_panelArea->GetPage(index);

    if (auto* entry = FindEntry(window)) {
        ClosePanel(*entry);
    }
}

void PanelHandler::HandleMenuEvents(wxCommandEvent &event) {
    auto id = event.GetId();
    auto* entry = FindEntry(id);
    if (entry == nullptr) {
        event.Skip();
        return;
    }

    if (event.IsChecked()) {
        if (!ShowPanel(*entry)) {
            return;
        }
    } else if (entry->panel != nullptr) {
        if (!ClosePanel(*entry)) {
            return;
        }
    }

    event.Skip();
}

bool PanelHandler::ShowPanel(Entry &entry) {
    if (entry.panel != nullptr) {
        return true;
    }

    entry.panel = entry.creator();
    if (entry.panel == nullptr) {
        return false;
    }

    entry.window = entry.panel->ShowPanel();
    if (entry.window == nullptr) {
        GetCmdMgr().Check(entry.id, false);
        if (entry.managed) {
            delete entry.panel;
        }
        entry.panel = nullptr;
        return false;
    }
    GetCmdMgr().Check(entry.id, true);

    m_panelArea->AddPage(entry.window, GetLang("panel." + entry.name + ".title"), true);

    m_visibleCount += 1;
    if (m_visibleCount == 1) {
        m_aui->GetPane(m_panelArea).Show(true);
        m_aui->Update();
    }
    return true;
}

bool PanelHandler::ClosePanel(Entry &entry) {
    if (entry.panel == nullptr) {
        return true;
    }

    if (!entry.panel->HidePanel()) {
        GetCmdMgr().Check(entry.id, true);
        return false;
    }
    GetCmdMgr().Check(entry.id, false);

    auto index = m_panelArea->GetPageIndex(entry.window);
    m_panelArea->RemovePage(index);
    if (entry.managed) {
        delete entry.window;
    }
    entry.window = nullptr;

    if (entry.managed) {
        delete entry.panel;
    }
    entry.panel = nullptr;

    m_visibleCount -= 1;
    if (m_visibleCount == 0) {
        m_aui->GetPane(m_panelArea).Hide();
        m_aui->Update();
    }

    return true;
}

PanelHandler::Entry* PanelHandler::Register(const wxString &name, int id, PanelHandler::PanelCreatorFn creator) {
    if (IsRegistered(name)) {
        wxLogWarning("Panel '" + name + "' is already registered with PanelHandler"); // NOLINT
        return nullptr;
    }

    auto& cmdMgr = GetCmdMgr();
    if (cmdMgr.FindEntry(id) == nullptr) {
        cmdMgr.Register(name, CmdManager::Entry{id, CmdManager::Type::Check, false, true, nullptr});
    }

    return &m_entries.emplace(name, Entry{name, id, std::move(creator)}).first->second;
}

PanelHandler::Entry* PanelHandler::FindEntry(int id) noexcept {
    auto iter = std::find_if(m_entries.begin(), m_entries.end(), [id](const auto& e){
        return e.second.id == id;
    });
    return iter == m_entries.end() ? nullptr : &iter->second;
}

PanelHandler::Entry* PanelHandler::FindEntry(wxWindow *wnd) noexcept {
    auto iter = std::find_if(m_entries.begin(), m_entries.end(), [wnd](const auto& e){
        return e.second.window == wnd;
    });
    return iter == m_entries.end() ? nullptr : &iter->second;
}
