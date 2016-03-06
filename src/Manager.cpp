/**
 * FBIde project
 */
#include "app_pch.hpp"
#include "Manager.hpp"
#include "UiManager.hpp"
#include "ConfigManager.hpp"
#include "CmdManager.hpp"


using namespace fbide;


// get manager instance
Manager & fbide::GetMgr()
{
    return Manager::GetInstance();
}

// shorthand to retreave ui manager
UiManager & fbide::GetUiMgr()
{
    return GetMgr().GetUiManager();
}

// shorthand to retreave ui manager
ConfigManager & fbide::GetCfgMgr()
{
    return GetMgr().GetConfigManager();
}

// shorthand to get config
Config & fbide::GetConfg()
{
    return GetCfgMgr().Get();
}

// Shortcut the get the langauge
Config & fbide::GetLang()
{
    return GetCfgMgr().Lang();
}

// shorthand to get translated string
const wxString & fbide::GetLang(const wxString & path, const wxString def)
{
    return GetLang().Get(path, def);
}

// get command manager
CmdManager & fbide::GetCmdMgr()
{
    return GetMgr().GetCmdManager();
}

//------------------------------------------------------------------------------
// Manager basics
//------------------------------------------------------------------------------

namespace {
    static Manager * p_instance = nullptr;
}


// Setup the manager
Manager::Manager()
{
}


// clean up. Managers must shut down
// in defined order
Manager::~Manager()
{
    if (m_ui)  m_ui.reset();
    if (m_cmd) m_cmd.reset();
    if (m_cfg) m_cfg.reset();
}


// Load stuff
void Manager::Load()
{
    m_cfg = std::make_unique<ConfigManager>();
    m_cmd = std::make_unique<CmdManager>();
    m_ui  = std::make_unique<UiManager>();
}


/**
 * Get global manager instance
 */
Manager & Manager::GetInstance()
{
    if (p_instance == nullptr) {
        p_instance = new Manager();
    }
    return *p_instance;
}


/**
 * Release global manager instance
 */
void Manager::Release()
{
    if (p_instance != nullptr) {
        delete p_instance;
        p_instance = nullptr;
    }
}


//------------------------------------------------------------------------------
// Retreave amanagers
//------------------------------------------------------------------------------

// UI
UiManager & Manager::GetUiManager()
{
    return *m_ui;
}


// config
ConfigManager & Manager::GetConfigManager()
{
    return *m_cfg;
}


// cmd
CmdManager & Manager::GetCmdManager()
{
    return *m_cmd;
}
