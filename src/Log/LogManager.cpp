//
// Created by Albert on 7/27/2020.
//
#include "LogManager.hpp"
#include "App/Manager.hpp"
#include "Config/ConfigManager.hpp"
#include "UI/UiManager.hpp"
#include "UI/PanelHandler.hpp"
using namespace fbide;

static const int ID_ToggleLog = ::wxNewId();

LogManager::LogManager() {
    auto& uiMgr = GetUiMgr();
    auto* panelHandler = uiMgr.GetPanelHandler();
    auto* entry = panelHandler->Register("toggle_log", ID_ToggleLog, [this]() { return this; });
    if (entry == nullptr) {
        wxLogError("Failed to register panel with PanelHandler");
        return;
    }

    auto* panelArea = panelHandler->GetPanelArea();
    auto style = wxTE_MULTILINE | wxTE_READONLY;
    m_textCtrl = new wxTextCtrl(panelArea, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, style);
    m_textCtrl->Hide();

    m_textCtrl->Bind(wxEVT_TEXT, [this, panelHandler, entry](auto){
        if (!m_textCtrl->IsShown()) {
            panelHandler->ShowPanel(*entry);
        }
    });

    m_log = std::make_unique<wxLogTextCtrl>(m_textCtrl);
    wxLog::SetActiveTarget(m_log.get());
}

LogManager::~LogManager() {
    wxLog::SetActiveTarget(nullptr);
}

wxWindow* LogManager::ShowPanel() {
    if (m_textCtrl != nullptr) {
        m_textCtrl->Show();
    }
    return m_textCtrl;
}

bool LogManager::HidePanel() {
    if (m_textCtrl != nullptr) {
        m_textCtrl->Clear();
        m_textCtrl->Hide();
    }
    return true;
}
