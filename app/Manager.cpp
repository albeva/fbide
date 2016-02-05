#include "app_pch.h"
#include "Manager.h"

namespace {
    static Manager * p_instance = nullptr;
}

Manager::Manager()
{
}


Manager::~Manager()
{
}


/**
 * Get global manager instance
 */
Manager & Manager::Get()
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