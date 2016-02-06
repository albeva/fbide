/**
 * FBIde project
 */
#include "app_pch.hpp"

#include "Manager.hpp"
#include "UiManager.hpp"
#include "ConfigManager.hpp"

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

//------------------------------------------------------------------------------
// Manager basics
//------------------------------------------------------------------------------

namespace {
    static Manager * p_instance = nullptr;
}


// Setup the manager
Manager::Manager() : m_cfg(nullptr), m_ui(nullptr)
{
}


// clean up
Manager::~Manager()
{
    if (m_cfg) {
        delete m_cfg;
        m_cfg = nullptr;
    }
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
    if (m_ui == nullptr) {
        m_ui = new UiManager();
    }
    return *m_ui;
}


// config
ConfigManager & Manager::GetConfigManager()
{
    if (m_cfg == nullptr) {
        m_cfg = new ConfigManager();
    }
    return *m_cfg;
}
