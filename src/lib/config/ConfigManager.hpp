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

/**
 * Multi-file INI store + path resolver.
 *
 * Five categories (`config`, `locale`, `shortcuts`, `keywords`,
 * `layout`) each back a separate file but share one Value tree shape.
 * Reads are typed via `Value::value_or` / `get_or`; writes mutate the
 * tree and `save(category)` flushes one category to disk. The active
 * editor `Theme` is held directly (typed schema, not `Value`).
 *
 * **Owns:** the five `Value` trees + the active `Theme`.
 * **Owned by:** `Context` (constructed first, destroyed last).
 * **Threading:** UI thread only.
 * **Lifecycle:** `App::OnInit` builds it with the resolved binary,
 * IDE, and config paths; `reloadConfig` / `setCategoryPath` rotate
 * a single file at runtime.
 *
 * See @ref settings for the user-facing edit chain and hot-reload
 * cascade.
 */
class ConfigManager final {
public:
    NO_COPY_AND_MOVE(ConfigManager)

    // -----------------------------------------------------------------------
    // Config categories
    // -----------------------------------------------------------------------

    /// Logical config category — one per backing INI file.
    enum class Category : std::uint8_t {
        Config,    ///< Top-level user settings (`config_<plat>.ini`).
        Locale,    ///< Display strings (`locales/<lang>.ini`).
        Shortcuts, ///< Keyboard accelerators (`shortcuts_<plat>.ini`).
        Keywords,  ///< FreeBASIC keyword groups (`keywords.ini`).
        Layout,    ///< Menu / toolbar wiring (`layout.ini`).
    };

    /// Stable string name for a category — matches `--cfg=<prefix>:`.
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

    /// Platform default config file name.
    [[nodiscard]] static auto getPlatformConfigFileName() -> wxString;

    /// Platform default terminal command (for running programs).
    [[nodiscard]] static auto getTerminal() -> wxString;

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    /// Construct and load every category from disk.
    /// @param appPath    Directory of the running fbide binary.
    /// @param idePath    Override for the `<binary>/ide` resource directory.
    /// @param configPath Override for the platform default config file
    ///                   (resolved relative to `idePath` when not absolute).
    explicit ConfigManager(const wxString& appPath, const wxString& idePath = "", const wxString& configPath = "");

    /// Point a category to a new file and reload it.
    void setCategoryPath(Category category, const wxString& path);

    /// Override the main config file path (used by --config) and reload.
    void reloadConfig(const wxString& configPath);

    /// Save the category's Value tree to its backing file.
    void save(Category category);

    /// Reload any loaded config category or the active theme whose backing
    /// file matches `path`. Returns true when a reload occurred. Use to
    /// hot-refresh IDE state after the user edits a config file in-place.
    auto reloadIfKnown(const wxString& path) -> bool;

    // -----------------------------------------------------------------------
    // Path management
    // -----------------------------------------------------------------------

    /// Resolve `pathName` to an absolute path against `appDir`.
    [[nodiscard]] auto absolute(const wxString& pathName) const -> wxString;
    /// Make `path` relative to `appDir` if possible.
    [[nodiscard]] auto relative(const wxString& path) const -> wxString;

    /// Application directory (resolved from `appPath` argument).
    [[nodiscard]] auto getAppDir() const -> const wxString& { return m_appDir; }
    /// IDE resources directory (e.g. `<appDir>/ide` by default).
    [[nodiscard]] auto getIdeDir() const -> const wxString& { return m_ideDir; }

    // -----------------------------------------------------------------------
    // Category accessors — return a reference to the category root Value.
    // -----------------------------------------------------------------------

    /// Look up the root `Value` for a category.
    [[nodiscard]] auto get(Category category) -> Value&;

    /// Shortcut for `get(Category::Config)`.
    [[nodiscard]] auto config() -> Value& { return get(Category::Config); }
    /// Shortcut for `get(Category::Locale)`.
    [[nodiscard]] auto locale() -> Value& { return get(Category::Locale); }
    /// Shortcut for `get(Category::Shortcuts)`.
    [[nodiscard]] auto shortcuts() -> Value& { return get(Category::Shortcuts); }
    /// Shortcut for `get(Category::Keywords)`.
    [[nodiscard]] auto keywords() -> Value& { return get(Category::Keywords); }
    /// Shortcut for `get(Category::Layout)`.
    [[nodiscard]] auto layout() -> Value& { return get(Category::Layout); }

    // -----------------------------------------------------------------------
    // File dialog wildcard patterns
    //
    // Each platform-specific config file has a `[filePatterns]` section with
    // named wildcard fragments (e.g. `freebasic`, `session`, `allFiles`).
    // Dialog code looks up one or more by name and joins with `|`.
    // -----------------------------------------------------------------------

    /// Look up a single `[filePatterns]` entry. Returns "" if absent.
    [[nodiscard]] auto filePattern(const wxString& key) -> wxString;

    /// Join multiple `[filePatterns]` entries into one wxFileDialog
    /// wildcard string. Missing or empty entries are skipped.
    [[nodiscard]] auto filePatterns(std::initializer_list<std::string_view> keys) -> wxString;

    // -----------------------------------------------------------------------
    // Theme (owned directly, not part of Value tree)
    // -----------------------------------------------------------------------

    /// Active editor theme.
    [[nodiscard]] auto getTheme() -> Theme& { return m_theme; }
    /// Const overload of `getTheme`.
    [[nodiscard]] auto getTheme() const -> const Theme& { return m_theme; }

private:
    /// Load the category file from disk and rebuild its Value tree.
    void load(Category category);

    /// Per-category bookkeeping: which file backs it and its parsed root.
    struct Entry final {
        Category category; ///< Category identifier.
        wxString path;     ///< Absolute path to the backing INI file.
        Value root;        ///< Parsed root `Value` for the category.
    };
    /// Number of categories — one slot per `Category` enum value.
    static constexpr std::size_t CAT_COUNT = 5;

    wxString m_appDir;                            ///< App directory (binary location).
    wxString m_ideDir {};                         ///< IDE resources directory.
    std::array<Entry, CAT_COUNT> m_categories {}; ///< Per-category state.
    Theme m_theme {};                             ///< Active editor theme.
};

} // namespace fbide
