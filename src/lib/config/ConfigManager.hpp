//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

class ConfigManager final {
public:
    NO_COPY_AND_MOVE(ConfigManager)

    /// Config value
    using ConfigValue = toml::basic_value<toml::ordered_type_config>;

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

    /// Load configuration from given path information
    /// If idePath is empty, then load fbidePath + "/ide"
    /// if configPath is not provided, load it from resolved ide path + "config_{platform}.toml"
    explicit ConfigManager(const wxString& appPath, const wxString& idePath = "", const wxString& configPath = "");

    /// Load category from given path.
    void load(Category category);

    /// Save given category
    void save(Category category);

    /// Resolve the path against ide folders and return absolute path
    [[nodiscard]] auto absolute(const wxString& pathName) const -> wxString;

    /// Turn absolute path relative to known paths
    [[nodiscard]] auto relative(const wxString& path) const -> wxString;

    /// Get toml for the category
    [[nodiscard]] auto get(Category category) -> ConfigValue&;
    [[nodiscard]] auto getConfig() -> ConfigValue& { return get(Category::Config); }
    [[nodiscard]] auto getLocale() -> ConfigValue& { return get(Category::Locale); }
    [[nodiscard]] auto getTheme() -> ConfigValue& { return get(Category::Theme); }
    [[nodiscard]] auto getShortcuts() -> ConfigValue& { return get(Category::Shortcuts); }
    [[nodiscard]] auto getKeywords() -> ConfigValue& { return get(Category::Keywords); }
    [[nodiscard]] auto getLayout() -> ConfigValue& { return get(Category::Layout); }

    template<typename T>
    [[nodiscard]] auto read(const wxString& path) -> std::optional<T> {
        if (const auto existing = read(path)) {
            return read<T>(*existing);
        }
        return std::nullopt;
    }

    [[nodiscard]] auto read(const wxString& path) -> std::optional<ConfigValue*>;

    template<typename T>
    [[nodiscard]] auto read_or(const wxString& path, const T& def) -> const T& {
        if (const auto cfg = read(path)) {
            if (const auto value = read<T>(*cfg.value())) {
                return *value;
            }
        }
        return def;
    }

    [[nodiscard]] auto read_or(const wxString& path, const int def) -> int {
        const ConfigValue::integer_type value = def;
        return static_cast<int>(read_or(path, value));
    }

    template<std::size_t N>
    [[nodiscard]] auto read_or(const wxString& path, const char (&def)[N]) -> std::string {
        const std::string str{def};
        return read_or(path, str);
    }

private:

    template<typename T>
    [[nodiscard]] static auto read(const ConfigValue& value) -> const T* {
        if constexpr (std::is_same_v<T, ConfigValue::boolean_type>) {
            return value.is_boolean() ? &value.as_boolean() : nullptr;
        } else if constexpr (std::is_same_v<T, ConfigValue::integer_type>) {
            return value.is_integer() ? &value.as_integer() : nullptr;
        } else if constexpr (std::is_same_v<T, ConfigValue::floating_type>) {
            return value.is_floating() ? &value.as_floating() : nullptr;
        } else if constexpr (std::is_same_v<T, ConfigValue::string_type>) {
            return value.is_string() ? &value.as_string() : nullptr;
        } else {
            static_assert(false, "Invalid value type for TOML");
            std::unreachable();
        }
    }

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
