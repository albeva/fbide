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
    
    
    auto c = Config();
    auto b = Config();
//    c = 12.5;
//    c = true;
    c = "hello";
//    c = 123;
//    b = c;

    if (c == 12.5) {
        std::cout << "c = 12.5\n";
    }
    if (c == 123) {
        std::cout << "c = 123\n";
    }
    if (c == true) {
        std::cout << "c = true\n";
    }
    if (c == wxString("hello")) {
        std::cout << "c = \"hello\"\n";
    }
    
    if (c == Config()) {
        std::cout << "c is null\n";
    }
    
    if (c == b) {
        std::cout << "c == b\n";
    }
    if (c != b) {
        std::cout << "c != b\n";
    }
}
