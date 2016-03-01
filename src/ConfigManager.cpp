//
//  ConfigManager.cpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "ConfigManager.hpp"


using namespace fbide;


// Load the configuration
ConfigManager::ConfigManager() : m_root(0)
{
}


// clean up
ConfigManager::~ConfigManager()
{
}


// Load configuration
void ConfigManager::Load(const wxString & path)
{
    if (!::wxFileExists(path)) {
        throw std::invalid_argument("fbide config file '" + path + "' not found");
    }
    m_root = Config::LoadYaml(path);
}
