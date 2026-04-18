//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Value.hpp"

namespace fbide {

class ConfigManager final {
public:
    NO_COPY_AND_MOVE(ConfigManager)

    /// Types of config this class manages
    enum class Category: std::uint8_t {
        Config,
        Locale,
        Theme,
        Shortcuts,
        Keywords,
        Layout,
    };

    [[nodiscard]] static constexpr auto getCategoryName(const Category category) -> std::string_view {
        switch (category) {
        case Category::Config: return "config";
        case Category::Locale: return "locale";
        case Category::Theme:  return "theme";
        case Category::Shortcuts: return "shortcuts";
        case Category::Keywords: return "keywords";
        case Category::Layout: return "layout";
        }
        std::unreachable();
    }

    /// Load configuration from given path information
    /// If idePath is empty, then load fbidePath + "/ide"
    /// if configPath is not provided, load it from resolved ide path + "config_{platform}.toml"
    explicit ConfigManager(const wxString& appPath, const wxString& idePath = "", const wxString& configPath = "");

    /// Load category from given path.
    void load(Category category);

    /// Save given category
    void save(Category category);

    /// Get toml for the category
    [[nodiscard]] auto get(Category category) -> Value;
    [[nodiscard]] auto getConfig() -> Value { return get(Category::Config); }
    [[nodiscard]] auto getLocale() -> Value { return get(Category::Locale); }
    [[nodiscard]] auto getTheme() -> Value { return get(Category::Theme); }
    [[nodiscard]] auto getShortcuts() -> Value { return get(Category::Shortcuts); }
    [[nodiscard]] auto getKeywords() -> Value { return get(Category::Keywords); }
    [[nodiscard]] auto getLayout() -> Value { return get(Category::Layout); }

private:
    /// Resolve the path against ide folders and return absolute path
    [[nodiscard]] auto absolute(const wxString& pathName) const -> wxString;

    /// Turn absolute path relative to known paths
    [[nodiscard]] auto relative(const wxString& path) const -> wxString;

    struct Entry final {
        Category category;
        wxString path;
        toml::value value;
    };
    static constexpr std::size_t CAT_COUNT = 6;

    wxString m_appDir;
    wxString m_ideDir{};
    std::array<Entry, CAT_COUNT> m_categories{};
};

} // fbide
