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
#include "ConfigManager.hpp"
#include "StyleEntry.hpp"

using namespace fbide;

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;


// Load configuration
void ConfigManager::Load(const wxString& basePath, const wxString& configFile) {
    if (!::wxFileExists(configFile)) {
        throw std::invalid_argument("fbide config file '" + configFile + "' not found");
    }
    m_root = Config::LoadYaml(configFile);

    // Important paths
    m_configPath = wxPathOnly(configFile);
    m_basePath = basePath;
    m_resourcePath = basePath / "ide";

    // Load IDE resources
    LoadLanguage();
    LoadKeywords();
    LoadStyle();

    LOG_VERBOSE("Loaded config from " + configFile);
}

void ConfigManager::LoadKeywords() {
    wxString keywordsFile = ResolveResourcePath(m_root.Get(Key::Keywords, "keywords.yaml"));
    if (!::wxFileExists(keywordsFile)) {
        throw std::invalid_argument("Keywords file not found. "s + keywordsFile.ToStdString());
    }
    const auto kwConfig = Config::LoadYaml(keywordsFile);
    if (kwConfig.IsArray()) {
        const auto& arr = kwConfig.AsArray();
        for (size_t i = 0; i < arr.size() && i < m_keywords.size(); i++) {
            const auto& kw = arr.at(i);
            if (kw.IsString()) {
                m_keywords.at(i) = kw.AsString();
            }
        }
    } else {
        throw std::invalid_argument("Invalid keywords file. "s + keywordsFile.ToStdString());
    }
    LOG_VERBOSE("Loaded keywords from " + keywordsFile);
}

void ConfigManager::LoadLanguage() {
    wxString langFile = ResolveResourcePath(m_root.Get(Key::AppLanguage, "lang.en.yaml"));
    if (!::wxFileExists(langFile)) {
        throw std::invalid_argument("Language file not found. "s + langFile.ToStdString());
    }
    m_lang = Config::LoadYaml(langFile);
    LOG_VERBOSE("Loaded language from " + langFile);
}

void ConfigManager::LoadStyle() {
    const auto& theme = GetTheme();
    constexpr std::array keys = {
        #define DEF_KEY(NAME) #NAME,
        FB_STYLE(DEF_KEY)
        #undef DEF_KEY
    };
    m_styles.reserve(keys.size());

    const auto& def = m_styles.emplace_back(theme, nullptr);
    for (size_t i = 1; i < keys.size(); i++) {
        m_styles.emplace_back(theme.GetOrEmpty(keys.at(i)), &def);
    }
}

Config& ConfigManager::GetTheme() noexcept {
    if (m_theme.IsNull()) {
        if (const auto *theme = m_root.Get("Editor.Theme")) {
            auto file = ResolveResourcePath(theme->AsString());
            m_theme = Config::LoadYaml(file);
            LOG_VERBOSE("Loaded theme from " + file);
        }
    }
    return m_theme;
}

Config ConfigManager::LoadConfigFromKey(const wxString& path, const wxString& def) const noexcept {
    const auto& name = m_root.Get(path, def);
    if (name.empty()) {
        return Config::Empty;
    }
    const auto file = ResolveResourcePath(name);
    if (!wxFileExists(path)) {
        return Config::Empty;
    }
    return Config::LoadYaml(file);
}

wxString ConfigManager::ResolveResourcePath(const wxString& path) const noexcept {
    if (wxIsAbsolutePath(path)) {
        return path;
    }

    wxFileName fileName = path;
    fileName.MakeAbsolute(m_configPath);
    if (fileName.Exists()) {
        return fileName.GetFullPath();
    }

    fileName = path;
    fileName.MakeAbsolute(m_resourcePath);
    if (fileName.Exists()) {
        return fileName.GetFullPath();
    }

    return path;
}
