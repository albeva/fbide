//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Theme.hpp"
#include "Value.hpp"
#include "Version.hpp"

namespace fbide {

class ConfigManager final {
public:
    NO_COPY_AND_MOVE(ConfigManager)

    // -----------------------------------------------------------------------
    // Config categories
    // -----------------------------------------------------------------------

    enum class Category : std::uint8_t {
        Config,
        Locale,
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
    // Get info
    // -----------------------------------------------------------------------

    /// Get paths of every language file under resources/IDE/v2/locales.
    [[nodiscard]] auto getAllLanguages() const -> std::vector<wxString>;

    /// Get paths of every theme file under resources/IDE/v2/themes.
    [[nodiscard]] auto getAllThemes() const -> std::vector<wxString>;

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    explicit ConfigManager(const wxString& appPath, const wxString& idePath = "", const wxString& configPath = "");

    /// Point a category to a new file and reload it.
    void setCategoryPath(Category category, const wxString& path);

    /// Save the category's Value tree to its backing file.
    void save(Category category);

    // -----------------------------------------------------------------------
    // Path management
    // -----------------------------------------------------------------------

    [[nodiscard]] auto absolute(const wxString& pathName) const -> wxString;
    [[nodiscard]] auto relative(const wxString& path) const -> wxString;

    // -----------------------------------------------------------------------
    // Category accessors — return a reference to the category root Value.
    // -----------------------------------------------------------------------

    [[nodiscard]] auto get(Category category) -> Value&;

    [[nodiscard]] auto config()    -> Value& { return get(Category::Config); }
    [[nodiscard]] auto locale()    -> Value& { return get(Category::Locale); }
    [[nodiscard]] auto shortcuts() -> Value& { return get(Category::Shortcuts); }
    [[nodiscard]] auto keywords()  -> Value& { return get(Category::Keywords); }
    [[nodiscard]] auto layout()    -> Value& { return get(Category::Layout); }

    // -----------------------------------------------------------------------
    // Theme (owned directly, not part of Value tree)
    // -----------------------------------------------------------------------

    [[nodiscard]] auto getTheme() -> Theme& { return m_theme; }
    [[nodiscard]] auto getTheme() const -> const Theme& { return m_theme; }

private:
    /// Load the category file from disk and rebuild its Value tree.
    void load(Category category);

    struct Entry final {
        Category category;
        wxString path;
        Value    root;
    };
    static constexpr std::size_t CAT_COUNT = 5;

    wxString m_appDir;
    wxString m_ideDir {};
    std::array<Entry, CAT_COUNT> m_categories {};
    Theme    m_theme {};
};

} // namespace fbide
