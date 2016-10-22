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
ConfigManager::ConfigManager() : m_root(0), m_lang(0)
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
    m_root.LoadYaml(path);
    
    // set IDE path
    auto idePath = wxPathOnly(path);
    m_root["IdePath"] = idePath;
    
    // Load language
    auto lang = m_root["App.Language"].AsString();
    if (!lang.IsEmpty()) {
        auto file = idePath + "/lang." + lang + ".yaml";
        if (!::wxFileExists(file)) {
            throw std::invalid_argument("Language file not found."s + file.ToStdString());
        }
        m_lang.LoadYaml(file);
    }
}
