//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ThemeCategory.hpp"
#include "Version.hpp"
// clang-format off
namespace fbide {

// Property triples for the extra categories from ThemeCategory.hpp.
// Kept here (not in ThemeCategory.hpp) because it references the
// nested Theme::Colors / Theme::Entry types.
#define DEFINE_THEME_EXTRA_PROPERTY(_)     \
    /* name        member      type     */ \
    _( LineNumber, lineNumber, Colors    ) \
    _( Selection,  selection,  Colors    ) \
    _( FoldMargin, foldMargin, Colors    ) \
    _( Brace,      brace,      Entry     ) \
    _( BadBrace,   badBrace,   Entry     )

#define DEFINE_THEME_PROPERTY(_)           \
    /* name        member      type     */ \
    _( Version,    version,    Version   ) \
    _( Separator,  separator,  wxColour  ) \
    _( Font,       font,       wxString  ) \
    _( FontSize,   fontSize,   int       ) \
    DEFINE_THEME_EXTRA_PROPERTY(_)

/**
 * Editor color theme — per-`ThemeCategory` styling plus top-level
 * properties (font, separator, line numbers, selection, fold margin,
 * brace match).
 *
 * Schema is fixed; `ThemeCategory` and the X-macros at the top of this
 * header generate the enum, accessors, and load/save in lock-step. To
 * add a new style slot, add one line to `DEFINE_THEME_CATEGORY` (or
 * one line to `DEFINE_THEME_PROPERTY` for a new top-level field).
 *
 * **Owned by:** `ConfigManager` (outside the `Value` tree because of
 * the typed schema).
 * **Files:** canonical `.ini` (v5+); legacy `.fbt` (v4) is read-only
 * and migrated through a v5 save.
 *
 * See @ref theming.
 */
class Theme final {
public:
    /// Default-constructed theme — every entry zero-initialised.
    Theme() = default;
    /// Copy-constructible.
    Theme(const Theme&) noexcept = default;
    /// Move-constructible.
    Theme(Theme&&) noexcept = default;
    /// Copy-assignable.
    auto operator=(const Theme&) -> Theme& = default;
    /// Move-assignable.
    auto operator=(Theme&&) -> Theme& = default;
    /// Defaulted equality (every member compared).
    auto operator==(const Theme&) const noexcept -> bool = default;

    /// Construct and load from `themePath` (`.ini` or legacy `.fbt`).
    explicit Theme(const wxString& themePath);

    /// Reload from `themePath`, replacing every member. Empty argument
    /// reloads from the current `m_themePath`.
    void load(const wxString& themePath = wxEmptyString) { load(themePath, true); }

    /// Load a legacy v4 `.fbt` theme (read-only migration; does not store path).
    void loadV4(const wxString& themePath);

    /// Save theme to disk. Empty argument saves to the current `m_themePath`.
    void save(const wxString& newThemePath = wxEmptyString);

    /// Current backing file path.
    [[nodiscard]] auto getPath() const -> const wxString& { return m_themePath; }

    /// Background + foreground colour pair for a category.
    struct Colors final {
        wxColour foreground; ///< Text colour.
        wxColour background; ///< Background colour.
        /// Defaulted equality.
        auto operator==(const Colors& other) const noexcept -> bool = default;
    };

    // -----------------------------------------------------------------------
    // Category entries
    // -----------------------------------------------------------------------

    /// Per-`ThemeCategory` styling: colours plus typeface flags.
    struct Entry final {
        Colors colors;          ///< Foreground + background colours.
        bool bold = false;      ///< Bold typeface.
        bool italic = false;    ///< Italic typeface.
        bool underlined = false;///< Underlined.
        /// Defaulted equality.
        auto operator==(const Entry& other) const noexcept -> bool = default;
    };

    /// Read the entry for a category.
    [[nodiscard]] auto get(const ThemeCategory category) const -> const Entry& {
        return m_categories[static_cast<std::size_t>(category)];
    }

    /// Replace the entry for a category.
    void set(const ThemeCategory category, const Entry& entry) {
        m_categories[static_cast<std::size_t>(category)] = entry;
    }

    // -----------------------------------------------------------------------
    // Style property getters and setters
    // -----------------------------------------------------------------------
    #define FUNCS(GETTER, MEMBER, TYPE)                                                \
        [[nodiscard]] auto get## GETTER() const -> const TYPE& { return m_## MEMBER; } \
        void set## GETTER(const TYPE& MEMBER) { m_## MEMBER = MEMBER; }
        DEFINE_THEME_PROPERTY(FUNCS)
    #undef FUNCS

    // -----------------------------------------------------------------------
    // Utility methods
    // -----------------------------------------------------------------------

    /// Return `color` if valid, otherwise the default-category foreground.
    [[nodiscard]] auto foreground(const wxColour& color) const -> const wxColour&;
    /// Return `color` if valid, otherwise the default-category background.
    [[nodiscard]] auto background(const wxColour& color) const -> const wxColour&;

private:
    /// Internal load entry — `reset` controls whether existing fields clear first.
    void load(const wxString& themePath, bool reset);

    wxString m_themePath;                                ///< Current backing file path.
    std::array<Entry, kThemeCategoryCount> m_categories {}; ///< Per-`ThemeCategory` style entries.

    // -----------------------------------------------------------------------
    // Style properties
    // -----------------------------------------------------------------------
    #define MEMBERS(GETTER, MEMBER, TYPE) TYPE m_## MEMBER {};
        DEFINE_THEME_PROPERTY(MEMBERS)
    #undef MEMBERS
};

} // namespace fbide
