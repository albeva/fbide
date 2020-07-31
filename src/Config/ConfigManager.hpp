//
//  ConfigManager.hpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "pch.h"
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
    inline Config& Get() noexcept { return m_root; }

    /**
     * Get language
     */
    inline Config& Lang() noexcept { return m_lang; }

    /**
     * Get Theme
     */
    Config& GetTheme() noexcept;

    /**
     * Load file at specified path
     */
    void Load(const wxString& path);

private:
    Config m_root;
    Config m_lang;
    Config m_theme;
};

} // namespace fbide
