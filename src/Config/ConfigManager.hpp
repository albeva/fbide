/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
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
