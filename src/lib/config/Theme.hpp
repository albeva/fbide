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

class Theme final {
public:
    // Default plumbing
    Theme() = default;
    Theme(const Theme&) noexcept = default;
    Theme(Theme&&) noexcept = default;
    auto operator=(const Theme&) -> Theme& = default;
    auto operator=(Theme&&) -> Theme& = default;
    auto operator==(const Theme&) const noexcept -> bool = default;

    /// Load from given theme file
    explicit Theme(const wxString& themePath);

    /// Reload theme from file, will reset all content
    void load(const wxString& themePath = wxEmptyString) { load(themePath, true); }

    /// Load legacy v4 .fbt theme (read-only migration; does not store path)
    void loadV4(const wxString& themePath);

    /// Save theme, Optionally to a new path
    void save(const wxString& newThemePath = wxEmptyString);

    /// Current backing file path
    [[nodiscard]] auto getPath() const -> const wxString& { return m_themePath; }

    /// Background and foreground colour combo
    struct Colors final {
        wxColour foreground;
        wxColour background;
        auto operator==(const Colors& other) const noexcept -> bool = default;
    };

    // -----------------------------------------------------------------------
    // Category entries
    // -----------------------------------------------------------------------

    struct Entry final {
        Colors colors;
        bool bold = false;
        bool italic = false;
        bool underlined = false;
        auto operator==(const Entry& other) const noexcept -> bool = default;
    };

    [[nodiscard]] auto get(const ThemeCategory category) const -> const Entry& {
        return m_categories[static_cast<std::size_t>(category)];
    }

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

    /// Return current colour if valid, or default color
    [[nodiscard]] auto foreground(const wxColour& color) const -> const wxColour&;
    [[nodiscard]] auto background(const wxColour& color) const -> const wxColour&;

private:
    void load(const wxString& themePath, bool reset);

    wxString m_themePath;
    std::array<Entry, kThemeCategoryCount> m_categories {};

    // -----------------------------------------------------------------------
    // Style properties
    // -----------------------------------------------------------------------
    #define MEMBERS(GETTER, MEMBER, TYPE) TYPE m_## MEMBER {};
        DEFINE_THEME_PROPERTY(MEMBERS)
    #undef MEMBERS
};

} // namespace fbide
