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
#include "Manager.hpp"
#include "Config/ConfigManager.hpp"
#include "Document/TypeManager.hpp"
#include "Document/DocumentManager.hpp"
#include "UI/CmdManager.hpp"
#include "UI/UiManager.hpp"
#include "Log/LogManager.hpp"
using namespace fbide;

//------------------------------------------------------------------------------
// Shortcuts
//------------------------------------------------------------------------------

// get manager instance
Manager& fbide::GetMgr() {
    return Manager::GetInstance();
}

// shorthand to UI manager
UiManager& fbide::GetUiMgr() {
    return GetMgr().GetUiManager();
}

// shorthand to Cfg manager
ConfigManager& fbide::GetCfgMgr() {
    return GetMgr().GetConfigManager();
}

// shorthand to Type manager
TypeManager& fbide::GetTypeMgr() {
    return GetMgr().GetTypeManager();
}

// shorthand to Document manager
DocumentManager& fbide::GetDocMgr() {
    return GetMgr().GetDocManager();
}

// shorthand to get config
Config& fbide::GetConfig() {
    return GetCfgMgr().Get();
}

// shorthand to get config
Config& fbide::GetConfig(const wxString& path) {
    return GetConfig()[path];
}

// Shortcut the get the language
Config& fbide::GetLang() {
    return GetCfgMgr().Lang();
}

// shorthand to get translated string
// BAD: default value captured and returned by reference!
const wxString& fbide::GetLang(const wxString& path, const wxString& def) {
    if (const auto *node = GetLang().Get(path)) {
        if (node->IsString()) {
            return node->AsString();
        }
    }
    return def;
}

// Get translated string and replace placeholders
wxString fbide::GetLang(const wxString& path, const StringMap<wxString>& map, const wxString& def) {
    auto str = GetLang(path, def);
    for (const auto& iter : map) {
        str.Replace("{" + iter.first + "}", iter.second, true);
    }
    return str;
}

// get command manager
CmdManager& fbide::GetCmdMgr() {
    return GetMgr().GetCmdManager();
}

//------------------------------------------------------------------------------
// Manager basics
//------------------------------------------------------------------------------

Manager* Manager::m_instance = nullptr; // NOLINT

// Setup the manager
Manager::Manager() = default;

// clean up. Managers must shut down
// in defined order
Manager::~Manager() {
    m_doc.reset();
    m_log.reset();
    if (m_ui) {
        m_ui->Unload();
    }
    m_ui.reset();
    m_cmd.reset();
    m_type.reset();
    m_cfg.reset();
}


// Load stuff
void Manager::Load() {
    m_cfg = std::make_unique<ConfigManager>();
    m_cmd = std::make_unique<CmdManager>();
    m_type = std::make_unique<TypeManager>();
    m_ui = std::make_unique<UiManager>();
    m_log = std::make_unique<LogManager>();
    m_doc = std::make_unique<DocumentManager>();
}

/**
 * Get global manager instance
 */
Manager& Manager::GetInstance() {
    if (m_instance == nullptr) {
        m_instance = new Manager(); // NOLINT
    }
    return *m_instance;
}

/**
 * Release global manager instance
 */
void Manager::Release() {
    if (m_instance != nullptr) {
        delete m_instance; // NOLINT
        m_instance = nullptr;
    }
}

//------------------------------------------------------------------------------
// Retrieve managers
//------------------------------------------------------------------------------

// UI
UiManager& Manager::GetUiManager() {
    return *m_ui;
}

// config
ConfigManager& Manager::GetConfigManager() {
    return *m_cfg;
}

// cmd
CmdManager& Manager::GetCmdManager() {
    return *m_cmd;
}

// type
TypeManager& Manager::GetTypeManager() {
    return *m_type;
}

LogManager &Manager::GetLogManager() {
    return *m_log;
}

DocumentManager &Manager::GetDocManager() {
    return *m_doc;
}
