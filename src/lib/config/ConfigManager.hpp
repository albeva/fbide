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

    using ConfigValue = Value::Inner;

    // -----------------------------------------------------------------------
    // Config categories
    // -----------------------------------------------------------------------

    /// Types of config this class manages
    enum class Category : std::uint8_t {
        Config,
        Locale,
        Theme,
        Shortcuts,
        Keywords,
        Layout,
    };

    [[nodiscard]] static constexpr auto getCategoryName(const Category category) -> std::string_view {
        switch (category) {
        case Category::Config:
            return "config";
        case Category::Locale:
            return "locale";
        case Category::Theme:
            return "theme";
        case Category::Shortcuts:
            return "shortcuts";
        case Category::Keywords:
            return "keywords";
        case Category::Layout:
            return "layout";
        }
        std::unreachable();
    }

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    /// Load configuration from given path information
    /// If idePath is empty, then load fbidePath + "/ide"
    /// if configPath is not provided, load it from resolved ide path + "config_{platform}.toml"
    explicit ConfigManager(const wxString& appPath, const wxString& idePath = "", const wxString& configPath = "");

    /// Load category from given path.
    void load(Category category);

    /// Save given category
    void save(Category category);

    // -----------------------------------------------------------------------
    // Path management
    // -----------------------------------------------------------------------

    /// Resolve the path against ide folders and return absolute path
    [[nodiscard]] auto absolute(const wxString& pathName) const -> wxString;

    /// Turn absolute path relative to known paths
    [[nodiscard]] auto relative(const wxString& path) const -> wxString;

    // -----------------------------------------------------------------------
    // Category accessors — return a Value cursor at the category root.
    // -----------------------------------------------------------------------

    [[nodiscard]] auto get(Category category) -> Value;

    [[nodiscard]] auto config()    -> Value { return get(Category::Config); }
    [[nodiscard]] auto locale()    -> Value { return get(Category::Locale); }
    [[nodiscard]] auto theme()     -> Value { return get(Category::Theme); }
    [[nodiscard]] auto shortcuts() -> Value { return get(Category::Shortcuts); }
    [[nodiscard]] auto keywords()  -> Value { return get(Category::Keywords); }
    [[nodiscard]] auto layout()    -> Value { return get(Category::Layout); }

private:
    struct Entry final {
        Category category;
        wxString path;
        ConfigValue value;
    };
    static constexpr std::size_t CAT_COUNT = 6;

    wxString m_appDir;
    wxString m_ideDir {};
    std::array<Entry, CAT_COUNT> m_categories {};
};

} // namespace fbide
