//
//  ConfigManager.hpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"
#include "Config.hpp"

namespace fbide {

/**
 * Global config keys
 */
namespace Key {
    constexpr auto IdePath      = "IdePath";
    constexpr auto BasePath     = "BasePath";
    constexpr auto AppLanguage  = "App.Language";
}

/**
 * Config mnager is responsible for saving/loading
 * application settings.
 *
 * It also supports API to add config settings by
 * various other components and potentially plugins
 */
class ConfigManager final {
    NON_COPYABLE(ConfigManager)
public:

    ConfigManager() = default;
    ~ConfigManager() = default;

    /**
     * Get main configuration root object
     */
    inline Config& Get() { return m_root; }

    /**
     * Get language
     */
    inline Config& Lang() { return m_lang; }

    /**
     * Load file at specified path
     */
    void Load(const wxString& path);

private:
    Config m_root;
    Config m_lang;
};

} // namespace fbide
