//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigManager.hpp"
using namespace fbide;

ConfigManager::ConfigManager(const wxString& appPath, const wxString& idePath, const wxString& configPath)
: m_appDir(appPath) {
    // resolve appPath
    if (not wxDirExists(appPath)) {
        wxLogError("app directory '%s' does not exist", appPath);
        return;
    }

    // Resolve ide/ path
    if (not idePath.empty()) {
        const auto path = absolute(idePath);
        if (wxDirExists(path)) {
            m_ideDir = path;
        } else {
            wxLogWarning("ide config directory '%s' does not exist", path);
        }
    }
    if (m_ideDir.empty()) {
        m_ideDir = m_appDir / "IDE" / "v2";
    }
    if (not wxDirExists(m_ideDir)) {
        wxLogError("ide config directory '%s' does not exist", m_ideDir);
        return;
    }

    // Initialize main config
    auto& entry = m_categories[static_cast<std::size_t>(Category::Config)];
    entry.path = absolute(configPath.empty() ? "config_win.toml"_wx : configPath);
    load(Category::Config);
}

void ConfigManager::load(const Category category) {
    auto& entry = m_categories[static_cast<std::size_t>(category)];
    wxString file;

    if (category == Category::Config) {
        file = entry.path;
    } else {
        const auto key = getCategoryName(category);
        const auto ref = config().at(std::string_view { key });
        if (!ref.isString()) {
            wxLogError("Config category '%s' missing or invalid", key.data());
            return;
        }
        file = absolute(wxString { ref.raw().as_string() });
    }

    if (!wxFileExists(file)) {
        wxLogError("Config file '%s' for '%s' category not found", file, getCategoryName(category).data());
        return;
    }

    try {
        entry.category = category;
        entry.path = file;
        entry.value = toml::parse<toml::ordered_type_config>(file.ToStdString(), toml::spec::v(1, 1, 0));
    } catch (const toml::exception& ex) {
        wxLogError(
            "Failed to parse toml file '%s' for category '%s', with error: %s",
            file, getCategoryName(category).data(), ex.what()
        );
    }
}

void ConfigManager::save(const Category category) {
    const auto& entry = m_categories[static_cast<std::size_t>(category)];
    if (entry.category != category) {
        wxLogWarning("Trying to save unloaded category '%s'", getCategoryName(category).data());
        return;
    }

    std::ofstream out(entry.path.ToStdString());
    out << entry.value;
}

auto ConfigManager::get(Category category) -> Value {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        load(category);
    }
    return Value { entry.value };
}

auto ConfigManager::absolute(const wxString& pathName) const -> wxString {
    wxFileName path(pathName);
    path.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_SHORTCUT);

    // already a full path
    if (path.IsAbsolute()) {
        return path.GetAbsolutePath();
    }

    wxFileName fn(path);

    // check against ide/ path
    fn.MakeAbsolute(m_ideDir);
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    // check against fbide path
    fn = path;
    fn.MakeAbsolute(m_appDir);
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    // check against cwd
    fn = path;
    fn.MakeAbsolute(wxGetCwd());
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    // No idea
    wxLogError("Failed to resolve absolute path %s", pathName);
    return pathName;
}

static auto makeRelative(const wxString& path, const wxString& to) -> std::optional<wxString> {
    if (to.StartsWith(path)) {
        wxFileName result(to);
        result.MakeRelativeTo(path);
        return result.GetFullPath();
    }
    return {};
}

auto ConfigManager::relative(const wxString& path) const -> wxString {
    const auto abs = absolute(path);
    if (const auto ide = makeRelative(m_ideDir, abs)) {
        return *ide;
    }
    if (const auto app = makeRelative(m_appDir, abs)) {
        return *app;
    }
    return path;
}
