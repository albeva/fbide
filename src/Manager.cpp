/**
 * FBIde project
 */
#ifdef _MSC_VER
    #include "app_pch.hpp"
#endif

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
Manager::Manager() :
    m_cfg(std::make_unique<ConfigManager>()),
    m_ui (std::make_unique<UiManager>())
{
}


// clean up
Manager::~Manager()
{
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
