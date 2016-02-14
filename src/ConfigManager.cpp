//
//  ConfigManager.cpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"

#include "ConfigManager.hpp"
#include "Value.hpp"


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
    

    Value v{true};
    
    if (v.IsBool()) {
        if (v != false) {
            std::cout << "v == true\n";
        }
        std::cout << "v is bool\n" << v.As<bool>();
    }
        
    
    
    if (v.IsNull()) {
        std::cout << "empty\n";
    }
    
    if (v.IsString()) {
        if (v == "std::string") {
            std::cout << "YAY\n";
        }
        std::cout << v.AsString();
    }
    
    
}






