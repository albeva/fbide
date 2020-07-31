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
#include "LogManager.hpp"
#include "App/Manager.hpp"
#include "Config/ConfigManager.hpp"
#include "UI/UiManager.hpp"
#include "UI/PanelHandler.hpp"
using namespace fbide;

static const int ID_ToggleLog = ::wxNewId(); // NOLINT
constexpr int DefaultLogFontSize = 12;

LogManager::LogManager() {
    auto& uiMgr = GetUiMgr();
    auto* panelHandler = uiMgr.GetPanelHandler();
    auto* entry = panelHandler->Register("toggle_log", ID_ToggleLog, [this]() { return this; });
    if (entry == nullptr) {
        wxLogError("Failed to register panel with PanelHandler"); // NOLINT
        return;
    }

    auto* panelArea = panelHandler->GetPanelArea();
    auto style = wxTE_MULTILINE | wxTE_READONLY; // NOLINT
    m_textCtrl = new wxTextCtrl(panelArea, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, style); // NOLINT
    m_textCtrl->Hide();

    wxFont font{
        DefaultLogFontSize,
        wxFONTFAMILY_MODERN,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL,
        false
    };
    font.SetSymbolicSize(wxFONTSIZE_MEDIUM);
    m_textCtrl->SetFont(font);

    auto handler = [this, panelHandler, entry](auto /* event */){
        if (!m_textCtrl->IsShown()) {
            panelHandler->ShowPanel(*entry);
        }
    };
    m_textCtrl->Bind(wxEVT_TEXT, handler);

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
