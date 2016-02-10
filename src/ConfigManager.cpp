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
ConfigManager::ConfigManager()
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
    
    // load YAML
}


// get value
wxAny & ConfigManager::Get(const std::string & key, const wxAny & def)
{
    for (auto & p : m_config) {
        if (p.first == key) {
            return p.second;
        }
    }
    return const_cast<wxAny&>(def);
}


// get const value
const wxAny & ConfigManager::Get(const std::string & key, const wxAny & def) const
{
    for (auto & p : m_config) {
        if (p.first == key) {
            return p.second;
        }
    }
    return const_cast<wxAny&>(def);
}


// read / write the key
wxAny & ConfigManager::operator[](const std::string & key)
{
    for (auto & p : m_config) {
        if (p.first == key) {
            return p.second;
        }
    }
    m_config[key] = wxAny();
    return m_config[key];
}

/**
 * Read const key
 */
const wxAny & ConfigManager::operator[](const std::string & key) const
{
    for (auto const & p : m_config) {
        if (p.first == key) {
            return p.second;
        }
    }
    throw InvalidKey(std::string("Invalid registry key: ") + key);
}

/**
 * Check if key exists
 */
bool ConfigManager::Has(const std::string & key) const
{
    return m_config.find(key) == m_config.end();
}

/**
 * Remove key
 */
bool ConfigManager::Remove(const std::string & key)
{
    return m_config.erase(key) != 0;
}
