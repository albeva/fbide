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

using namespace fbide;

// Load configuration
void ConfigManager::Load(const wxString& basePath, const wxString& configFile) {
    if (!::wxFileExists(configFile)) {
        throw std::invalid_argument("fbide config file '" + configFile + "' not found");
    }
    m_root = Config::LoadYaml(configFile);

    // set IDE path
    m_idePath = wxPathOnly(configFile);
    m_root[Key::IdePath] = m_idePath;

    // base path
    m_basePath = basePath;
    m_root[Key::BasePath] = basePath;

    // Load language
    auto lang = m_root[Key::AppLanguage].AsString();
    if (!lang.IsEmpty()) {
        auto file = ResolvePath("lang." + lang + ".yaml");
        if (!::wxFileExists(file)) {
            throw std::invalid_argument("Language file not found."s + file.ToStdString());
        }
        m_lang = Config::LoadYaml(file);
    }
}

Config& ConfigManager::GetTheme() noexcept {
    if (m_theme.IsNull()) {
        if (const auto *theme = m_root.Get("Editor.Theme")) {
            auto file = ResolvePath("themes" / theme->AsString() + ".yaml");
            m_theme = Config::LoadYaml(file);
            wxLogMessage("Editor theme loaded from: " + file); // NOLINT
        }
    }
    return m_theme;
}

wxString ConfigManager::ResolvePath(const wxString& path) const noexcept {
    if (wxIsAbsolutePath(path)) {
        return path;
    }

    wxFileName fileName = path;
    fileName.MakeAbsolute(m_idePath);
    if (fileName.Exists()) {
        return fileName.GetFullPath();
    }

    fileName = path;
    fileName.MakeAbsolute(m_basePath / "ide");
    if (fileName.Exists()) {
        return fileName.GetFullPath();
    }

    return path;
}
