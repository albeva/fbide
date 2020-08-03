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
#include "StyleEntry.hpp"

namespace fbide {

/**
 * Global config keys
 */
namespace Key {
    constexpr auto AppLanguage = "App.Language";
    constexpr auto Keywords = "Editor.Keywords";
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

    ConfigManager();
    ~ConfigManager();

    /**
     * Get main configuration root object
     */
    [[nodiscard]] inline Config& Get() noexcept { return m_root; }

    /**
     * Get language
     */
    [[nodiscard]] inline Config& Lang() noexcept { return m_lang; }

    /**
     * Get Theme
     */
    Config& GetTheme() noexcept;

    /**
     * Load file at specified path
     */
    void Load(const wxString& basePath, const wxString& configFile);

    /**
     * Get path where config file was loaded from
     */
    [[nodiscard]] const wxString& GetConfigPath() const noexcept { return m_configPath; }

    /**
     * App base path (executable path)
     */
    [[nodiscard]] const wxString& GetBasePath() const noexcept { return m_basePath; }

    /**
     * App path that contains resources. Usually: ide/
     */
    [[nodiscard]] const wxString& GetResourcePath() const noexcept { return m_resourcePath; }

    /**
     * Get FBIde keywords from config
     */
    [[nodiscard]] const auto& GetKeywords() const noexcept {
        return m_keywords;
    }

    /**
     * Get editor styles
     */
    [[nodiscard]] const auto& GetStyles() const noexcept {
        return m_styles;
    }

    /**
     * Resolve given relative path to either config path or resource path
     *
     * @param path relative path containing filename to find
     * @return path or a fully resolved absolute path
     */
    [[nodiscard]] wxString ResolveResourcePath(const wxString& path) const noexcept;

    /**
     * Get key from config and if exists, attempt to load file pointed to by the key
     * @return empty config if either key does not exist or file was not found
     */
    [[nodiscard]] Config LoadConfigFromKey(const wxString& path, const wxString& def = wxEmptyString) const noexcept;

private:

    void LoadKeywords();
    void LoadLanguage();
    void LoadStyle();

    wxString m_configPath;
    wxString m_basePath;
    wxString m_resourcePath;

    Config m_root;
    Config m_lang;
    Config m_theme;
    std::array<wxString, 4> m_keywords;
    std::vector<StyleEntry> m_styles;
};

} // namespace fbide
