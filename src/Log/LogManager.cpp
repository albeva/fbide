//
// Created by Albert on 7/27/2020.
//
#include "LogManager.hpp"
#include "App/Manager.hpp"
#include "Config/ConfigManager.hpp"
#include "UI/UiManager.hpp"
#include "UI/PanelHandler.hpp"
using namespace fbide;

LogManager::LogManager() {
    auto& uiMgr = GetUiMgr();
    auto* panelHandler = uiMgr.GetPanelHandler();
    auto* panelArea = panelHandler->GetPanelArea();

    auto style = wxTE_MULTILINE | wxTE_READONLY;
    m_textCtrl = new wxTextCtrl(panelArea, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, style);
    panelArea->AddPage(m_textCtrl, "Log", true);

    m_log = std::make_unique<wxLogTextCtrl>(m_textCtrl);
    wxLog::SetActiveTarget(m_log.get());

    /*
     * panelHandler->Register("log", [this]() { return this; });
     */
}

LogManager::~LogManager() {
    wxLog::SetActiveTarget(nullptr);
    m_log.reset();
}
