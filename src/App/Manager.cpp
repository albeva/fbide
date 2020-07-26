/**
 * FBIde project
 */
#include "Manager.hpp"
#include "Config/ConfigManager.hpp"
#include "Editor/TypeManager.hpp"
#include "UI/CmdManager.hpp"
#include "UI/UiManager.hpp"
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
    if (auto node = GetLang().Get(path)) {
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

Manager* Manager::m_instance = nullptr;

// Setup the manager
Manager::Manager() = default;

// clean up. Managers must shut down
// in defined order
Manager::~Manager() {
    if (m_ui) {
        m_ui->Unload();
        m_ui.reset();
    }
    if (m_cmd)
        m_cmd.reset();
    if (m_type)
        m_type.reset();
    if (m_cfg)
        m_cfg.reset();
}


// Load stuff
void Manager::Load() {
    m_cfg = std::make_unique<ConfigManager>();
    m_cmd = std::make_unique<CmdManager>();
    m_type = std::make_unique<TypeManager>();
    m_ui = std::make_unique<UiManager>();
}

/**
 * Get global manager instance
 */
Manager& Manager::GetInstance() {
    if (m_instance == nullptr) {
        m_instance = new Manager();
    }
    return *m_instance;
}

/**
 * Release global manager instance
 */
void Manager::Release() {
    if (m_instance != nullptr) {
        delete m_instance;
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
