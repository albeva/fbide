//
//  ConfigManager.cpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "ConfigManager.hpp"

using namespace fbide;

// Load configuration
void ConfigManager::Load(const wxString& path) {
    if (!::wxFileExists(path)) {
        throw std::invalid_argument("fbide config file '" + path + "' not found");
    }
    m_root.LoadYaml(path);

    // set IDE path
    auto idePath = wxPathOnly(path);
    m_root[Key::IdePath] = idePath;

    // base path
    auto basePath = idePath;
    #if defined(__DARWIN__)
        auto pos = basePath.find_last_of(".app");
        basePath.Remove(pos + 1, basePath.length() - pos);
    #elif defined(__WXMSW__)
        basePath.RemoveLast(4);
    #endif
    m_root[Key::BasePath] = basePath;

    // Load language
    auto lang = m_root[Key::AppLanguage].AsString();
    if (!lang.IsEmpty()) {
        auto file = idePath / "lang." + lang + ".yaml";
        if (!::wxFileExists(file)) {
            throw std::invalid_argument("Language file not found."s + file.ToStdString());
        }
        m_lang.LoadYaml(file);
    }
}
