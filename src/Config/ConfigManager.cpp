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
void ConfigManager::Load(const wxString& path) {
    if (!::wxFileExists(path)) {
        throw std::invalid_argument("fbide config file '" + path + "' not found");
    }
    m_root = Config::LoadYaml(path);

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
        m_lang = Config::LoadYaml(file);
    }
}

Config& ConfigManager::GetTheme() noexcept {
    if (m_theme.IsNull()) {
        if (const auto *theme = m_root.Get("Editor.Theme")) {
            const auto& idePath = m_root[Key::IdePath].AsString();
            auto file = idePath / "themes" / theme->AsString() + ".yaml";
            m_theme = Config::LoadYaml(file);
            wxLogMessage("Editor theme loaded from: " + file); // NOLINT
        }
    }
    return m_theme;
}
