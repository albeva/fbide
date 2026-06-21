//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"

namespace fbide {

#define DEFINE_SETTINGS_CATEGORY(_) \
    DEFINE_THEME_CATEGORY(_)        \
    DEFINE_THEME_EXTRA_PROPERTY(_)

/// Settings-UI category enum: all syntax styles, plus the extra Theme
/// properties (line number, selection, brace matching) — populated by
/// reusing the same x-macros that define Theme internals. The trailing
/// `Changes` slot is a hand-added pseudo-category for the diff-state
/// palette (Added / Modified / Removed / Background); it bypasses the
/// per-category `Colors`/`Entry` plumbing and is rendered by
/// `ThemePage` with its own custom picker block.
enum class SettingsCategory : int {
// clang-format off
    #define SYN_ENUM(NAME, ...) NAME,
        DEFINE_SETTINGS_CATEGORY(SYN_ENUM)
    #undef SYN_ENUM
    // clang-format on
    Changes,
};

constexpr auto operator+(const SettingsCategory& rhs) -> int {
    return static_cast<int>(rhs);
}

/// All settings categories (syntax + extras) enumerated
inline constexpr std::array kSettingsCategories {
// clang-format off
    #define SYN_ARR(NAME, ...) SettingsCategory::NAME,
        DEFINE_SETTINGS_CATEGORY(SYN_ARR)
    #undef SYN_ARR
    // clang-format on
};

inline constexpr std::size_t kSettingsCategoryCount = kSettingsCategories.size();

/// Get settings category name (syntax or extra)
constexpr auto getSettingsCategoryName(const SettingsCategory category) -> std::string_view {
    switch (category) {
        // clang-format off
        #define SYN_CASE(NAME)        case SettingsCategory::NAME:
        #define EXTRA_CASE(NAME, ...) case SettingsCategory::NAME: return #NAME;
            DEFINE_THEME_CATEGORY(SYN_CASE)
                return getThemeCategoryName(static_cast<ThemeCategory>(+category));
            DEFINE_THEME_EXTRA_PROPERTY(EXTRA_CASE)
        #undef SYN_CASE
        #undef EXTRA_CASE
        // clang-format on
    case SettingsCategory::Changes:
        return "Changes";
    }
    std::unreachable();
}

/// Is this settings category one of the syntax styles (vs an extra)?
constexpr auto isSyntaxCategory(const SettingsCategory category) -> bool {
    return +category < static_cast<int>(kThemeCategoryCount);
}

/// Locale-key name (under `dialogs.settings.themes.categories.*`)
/// for `cat`. Mirrors the labels the ThemePage's category tree uses
/// — keeping this here lets ColorPicker's "Copy from" submenu reuse
/// the same translations across all locale files instead of falling
/// back to raw `keywordTypes` / `numberPP`-style enum names.
constexpr auto getSettingsCategoryLabelKey(const SettingsCategory cat) -> std::string_view {
    switch (cat) {
    case SettingsCategory::Default:
        return "default";
    case SettingsCategory::Comment:
        return "comments";
    case SettingsCategory::MultilineComment:
        return "multilineComments";
    case SettingsCategory::Identifier:
        return "identifier";
    case SettingsCategory::Number:
        return "number";
    case SettingsCategory::String:
        return "string";
    case SettingsCategory::StringOpen:
        return "unterminated";
    case SettingsCategory::Keywords:
        return "core";
    case SettingsCategory::KeywordTypes:
        return "types";
    case SettingsCategory::KeywordOperators:
        return "operators";
    case SettingsCategory::KeywordConstants:
        return "defines";
    case SettingsCategory::KeywordLibrary:
        return "library";
    case SettingsCategory::KeywordCustom:
        return "custom";
    case SettingsCategory::KeywordPP:
        return "directives";
    case SettingsCategory::KeywordAsm1:
        return "instructions";
    case SettingsCategory::KeywordAsm2:
        return "registers";
    case SettingsCategory::Operator:
        return "operator";
    case SettingsCategory::Label:
        return "label";
    case SettingsCategory::Preprocessor:
        return "preprocessor";
    case SettingsCategory::NumberPP:
        return "ppNumber";
    case SettingsCategory::StringPP:
        return "ppString";
    case SettingsCategory::OperatorPP:
        return "ppOperator";
    case SettingsCategory::IdentifierPP:
        return "ppIdentifier";
    case SettingsCategory::Error:
        return "error";
    case SettingsCategory::LineNumber:
        return "lineNumbers";
    case SettingsCategory::Selection:
        return "selection";
    case SettingsCategory::WordHighlight:
        return "occurrences";
    case SettingsCategory::FoldMargin:
        return "fold";
    case SettingsCategory::Brace:
        return "match";
    case SettingsCategory::BadBrace:
        return "mismatch";
    case SettingsCategory::Changes:
        return "changes";
    }
    std::unreachable();
}

/// Capability descriptor — which UI controls apply to this category.
struct SettingsCapability final {
    bool foreground : 1; ///< Show the foreground colour picker.
    bool background : 1; ///< Show the background colour picker.
    bool style      : 1; ///< Show bold / italic / underlined toggles.
    bool font       : 1; ///< Show the font-face picker.
    bool fontSize   : 1; ///< Show the font-size spinner.
    bool separator  : 1; ///< Show the separator-line colour picker.
};

/// Read the theme entry backing a settings category, regardless of whether
/// it lives in the syntax-category array or is stored as an extra property.
inline auto readCategory(const Theme& theme, const SettingsCategory cat) -> Theme::Entry {
    if (isSyntaxCategory(cat)) {
        return theme.get(static_cast<ThemeCategory>(+cat));
    }
    switch (cat) {
        // clang-format off
        #define EXTRA_CASE(NAME, ...) case SettingsCategory::NAME: return { theme.get## NAME() };
            DEFINE_THEME_EXTRA_PROPERTY(EXTRA_CASE)
        #undef EXTRA_CASE
        // clang-format on
    case SettingsCategory::Changes:
        // `Changes` carries four wxColours, not a Colors/Entry — ThemePage
        // branches before this call. Reaching it here would mean a UI bug.
        std::unreachable();
    default:
        std::unreachable();
    }
}

/// Capability per settings category. Default syntax style carries the
/// editor-wide font + size; all other categories hide those controls.
constexpr auto capabilityOf(const SettingsCategory category) -> SettingsCapability {
    switch (category) {
    case SettingsCategory::Default:
        return { .foreground = true, .background = true, .style = true, .font = true, .fontSize = true, .separator = true };
    case SettingsCategory::LineNumber:
    case SettingsCategory::Selection:
    case SettingsCategory::WordHighlight:
    case SettingsCategory::FoldMargin:
        return { .foreground = true, .background = true, .style = false, .font = false, .fontSize = false, .separator = false };
    case SettingsCategory::Changes:
        // None of the standard pickers — ThemePage renders four
        // dedicated diff-state pickers in their place.
        return { .foreground = false, .background = false, .style = false, .font = false, .fontSize = false, .separator = false };
    default:
        // syntax styles (except Default) + Brace/BadBrace: colours + style
        return { .foreground = true, .background = true, .style = true, .font = false, .fontSize = false, .separator = false };
    }
}

// ---------------------------------------------------------------------------
// Category tree — single source of truth for both ThemePage's left-hand
// category tree and ColorPicker's "Copy from" submenu, so the two never
// drift.
// ---------------------------------------------------------------------------

/// One node in the theme settings category tree. A node either binds a
/// `SettingsCategory` (a selectable colour entry) or is a folder with a
/// hand-written `labelKey` (`keywords`, `margins`, … — labels that don't
/// map 1:1 to a category). Category nodes derive their locale key from
/// `getSettingsCategoryLabelKey`; folder nodes carry it explicitly. Both
/// resolve under `dialogs.settings.themes.categories.*`.
struct SettingsTreeNode final {
    std::optional<SettingsCategory> category;
    wxString labelKey; ///< Folder-only; empty when `category` is set.
    std::vector<SettingsTreeNode> children;

    /// Category-bound node — label key derived from the enum.
    // NOLINTNEXTLINE(google-explicit-constructor)
    SettingsTreeNode(const SettingsCategory cat, std::vector<SettingsTreeNode> kids = {})
    : category(cat)
    , children(std::move(kids)) {}

    /// Folder node — no category, explicit locale key.
    // NOLINTNEXTLINE(google-explicit-constructor)
    SettingsTreeNode(wxString key, std::vector<SettingsTreeNode> kids = {})
    : labelKey(std::move(key))
    , children(std::move(kids)) {}
};

/// The theme settings category tree (folders + category leaves). Drives
/// ThemePage's tree control and ColorPicker's "Copy from" submenu.
inline auto settingsCategoryTree() -> std::vector<SettingsTreeNode> {
    using SC = SettingsCategory;
    // Category-bound nodes are spelled `{ SC::Foo, { ... } }` and their
    // locale key is derived from the enum via `getSettingsCategoryLabelKey`.
    // Folder nodes use `{ "folderKey", { ... } }` (string literal picks the
    // wxString constructor) for labels that don't correspond to a single
    // category.
    // clang-format off
    return {
        { SC::Default, {
            { SC::Comment          },
            { SC::MultilineComment },
            { SC::Identifier       },
            { SC::Number           },
            { SC::String, {
                { SC::StringOpen },
            }},
            { SC::Operator         },
            { SC::Label            },
            { SC::Error            },
            { "keywords", {
                { SC::Keywords         },
                { SC::KeywordTypes     },
                { SC::KeywordOperators },
                { SC::KeywordConstants },
                { SC::KeywordLibrary   },
                { SC::KeywordCustom    },
            }},
            { "margins", {
                { SC::LineNumber },
                { SC::FoldMargin },
                { SC::Changes    },
            }},
            { SC::Selection },
            { SC::WordHighlight },
            { "brace", {
                { SC::Brace    },
                { SC::BadBrace },
            }},
        }},
        { "asm", {
            { SC::KeywordAsm1 },
            { SC::KeywordAsm2 },
        }},
        { SC::Preprocessor, {
            { SC::KeywordPP    },
            { SC::IdentifierPP },
            { SC::NumberPP     },
            { SC::StringPP     },
            { SC::OperatorPP   },
        }},
    };
    // clang-format on
}

} // namespace fbide
