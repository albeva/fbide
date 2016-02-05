/**
 * FBIde project
 */
#include "app_pch.h"
#include "Manager.h"
#include "UiManager.hpp"

using namespace fbide;


// get manager instance
Manager & fbide::GetMgr()
{
    return Manager::GetInstance();
}


UiManager & fbide::GetUiMgr()
{
    return GetMgr().GetUiManager();
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


// clean up
Manager::~Manager()
{
    UiManager::Release();
}


// Load the managers
void Manager::Load()
{
    UiManager::GetInstance();
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
UiManager & Manager::GetUiManager() const
{
    return UiManager::GetInstance();
}



