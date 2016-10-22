/**
 * FBIde project
 */
#include "app_pch.hpp"
#include "Manager.hpp"
#include "UiManager.hpp"
#include "ConfigManager.hpp"
#include "CmdManager.hpp"
#include "TypeManager.hpp"


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

// shorthand to retreave type manager
TypeManager & fbide::GetTypeMgr()
{
    return GetMgr().GetTypeManager();
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
const wxString & fbide::GetLang(const wxString & path, const wxString & def)
{
    return GetLang().Get(path, def);
}

// Get translated string and replace placeholders
wxString fbide::GetLang(const wxString & path,
                 const StringMap<wxString> & map,
                 const wxString & def)
{
    auto str = GetLang(path, def);
    for (const auto & iter : map) {
        str.Replace("{" + iter.first + "}", iter.second, true);
    }
    return str;
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
    Manager * p_instance = nullptr;
}


// Setup the manager
Manager::Manager()
{
}


// clean up. Managers must shut down
// in defined order
Manager::~Manager()
{
    if (m_ui) {
        m_ui->Unload();
        m_ui.reset();
    }
    if (m_type) m_type.reset();
    if (m_cmd)  m_cmd.reset();
    if (m_cfg)  m_cfg.reset();
}


// Load stuff
void Manager::Load()
{
    m_cfg  = std::make_unique<ConfigManager>();
    m_cmd  = std::make_unique<CmdManager>();
    m_type = std::make_unique<TypeManager>();
    m_ui   = std::make_unique<UiManager>();
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


// type
TypeManager & Manager::GetTypeManager()
{
    return *m_type;
}
