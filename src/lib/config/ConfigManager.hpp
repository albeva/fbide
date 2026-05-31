//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ConfigStrategy.hpp"
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

    /// Get paths of every theme file under `ide/themes/`. Under READONLY,
    /// also enumerates `<UserDataDir>/themes/`; user entries shadow
    /// bundle entries on basename collision. Sorted by basename.
    [[nodiscard]] auto getAllThemes() const -> std::vector<wxString>;

    /// Resolve a theme path (relative or absolute) honouring the
    /// READONLY two-dir model: when the sentinel is present, a file at
    /// `<UserDataDir>/<relPath>` takes precedence over the bundle copy.
    /// Falls back to `absolute()` (ide → app → cwd) when no user
    /// override exists. Absolute paths are returned as-is.
    [[nodiscard]] auto themePath(const wxString& relPath) const -> wxString;

    /// Directory theme files are written to, created if missing. Under
    /// READONLY this is `<UserDataDir>/themes/`; otherwise
    /// `<ideDir>/themes/`. Always safe to call before a theme write —
    /// `wxFileName::Mkdir(..., wxPATH_MKDIR_FULL)` is idempotent.
    [[nodiscard]] auto themesWriteDir() const -> wxString;

    /// Path to the recent-file-history INI, created parent dir if missing.
    /// Under READONLY this lives under `<UserDataDir>/`; otherwise next to
    /// the bundle in `<ideDir>/`. The `.local.ini` suffix marks it as
    /// user-local state, never shipped with the bundle.
    [[nodiscard]] auto historyPath() const -> std::filesystem::path;

    /// Platform default config file name.
    [[nodiscard]] static auto getPlatformConfigFileName() -> wxString;

    /// Platform default terminal command — opens a bare terminal window
    /// with no program inside (used by the "Open command prompt" action).
    [[nodiscard]] static auto getTerminal() -> wxString;

    /// Terminal launcher prefix for `<$terminal>` substitution in the run
    /// command. Reads `compiler.terminal` from config; falls back to a
    /// platform default. Empty on platforms where the launcher cannot be
    /// expressed as a command-line prefix (currently macOS).
    [[nodiscard]] auto getTerminalLauncher() -> wxString;

    /// Platform default for `compiler.terminal` — used when the config
    /// key is missing or empty.
    [[nodiscard]] static auto getDefaultTerminalLauncher() -> wxString;

    // -----------------------------------------------------------------------
    // Init
    // -----------------------------------------------------------------------

    /// Construct and load every category from disk.
    /// @param appPath              Directory of the running fbide binary.
    /// @param idePath              Override for the `<binary>/ide` resource directory.
    /// @param configPath           Override for the platform default config file
    ///                             (resolved relative to `idePath` when not absolute).
    /// @param userDataDirOverride  Test seam — overrides `wxStandardPaths::Get().GetUserDataDir()`
    ///                             for overlay routing under READONLY. Empty (default) uses
    ///                             the real platform user-data directory. Production code
    ///                             never passes this.
    explicit ConfigManager(
        const wxString& appPath,
        const wxString& idePath = "",
        const wxString& configPath = "",
        const wxString& userDataDirOverride = ""
    );

    /// Point a category to a new file and reload it.
    void setCategoryPath(Category category, const wxString& path);

    /// Override the main config file path (used by --config) and reload.
    void reloadConfig(const wxString& configPath);

    /// Save the category's Value tree to its backing file.
    void save(Category category) const;

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
    [[nodiscard]] auto getAppDir() const -> wxString;
    /// IDE resources directory (e.g. `<appDir>/ide` by default).
    [[nodiscard]] auto getIdeDir() const -> wxString;

    // -----------------------------------------------------------------------
    // Category accessors — return a reference to the category root Value.
    // -----------------------------------------------------------------------

    /// Look up the root `Value` for a category.
    [[nodiscard]] auto get(Category category) -> Value&;

    /// Pristine baseline for a category — the bundle file parsed without
    /// the user overlay merged in. Unlike `get()`/`config()` this is
    /// unaffected by user edits, so comparing a current value against it
    /// reveals whether the user diverged from the shipped default.
    [[nodiscard]] auto baseline(Category category) -> const Value&;

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

    /// Build the `ConfigStrategy` for `category` given a resolved base path.
    /// Uses `m_userDataDir`, `m_readOnlyIde`, `m_explicitConfig` so callers
    /// (ctor + `load()` for sub-categories + `reloadConfig` / `setCategoryPath`)
    /// share the same rules.
    [[nodiscard]] auto buildStrategy(Category category, const std::filesystem::path& basePath) const -> ConfigStrategy;

    /// Resolve `rel` to an absolute path, walking the same ide → app →
    /// cwd chain as the public `absolute()`. Used internally to avoid
    /// the round-trip through `wxString`.
    [[nodiscard]] auto absolutePath(const std::filesystem::path& rel) const -> std::filesystem::path;

    /// Per-category bookkeeping: storage policy + parsed trees.
    struct Entry final {
        Category category { Category::Config }; ///< Category identifier; overwritten on `load()`.
        ConfigStrategy strategy;                ///< Where the file lives and how saves are routed.
        Value baseline;                         ///< Pristine parse of `strategy.basePath()` — diffed against `root` on save.
        Value root;                             ///< Merged tree (baseline + overlay).
    };
    /// Number of categories — one slot per `Category` enum value.
    static constexpr std::size_t CAT_COUNT = 5;

    std::filesystem::path m_appDir;               ///< App directory (binary location).
    std::filesystem::path m_ideDir;               ///< IDE resources directory.
    std::filesystem::path m_userDataDir;          ///< User-writable dir for overlays + theme copies (READONLY mode).
    bool m_readOnlyIde { false };                 ///< True when `READONLY` sentinel routes overlays to `m_userDataDir`.
    bool m_explicitConfig { false };              ///< True when `--config=PATH` was supplied — all mutable categories use `Direct`.
    std::array<Entry, CAT_COUNT> m_categories {}; ///< Per-category state.
    Theme m_theme;                                ///< Active editor theme.
};

} // namespace fbide
