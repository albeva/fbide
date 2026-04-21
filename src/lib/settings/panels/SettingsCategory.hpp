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
/// reusing the same x-macros that define Theme internals.
enum class SettingsCategory : int {
// clang-format off
    #define SYN_ENUM(NAME, ...) NAME,
        DEFINE_SETTINGS_CATEGORY(SYN_ENUM)
    #undef SYN_ENUM
    // clang-format on
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
    }
    std::unreachable();
}

/// Is this settings category one of the syntax styles (vs an extra)?
constexpr auto isSyntaxCategory(const SettingsCategory category) -> bool {
    return +category < static_cast<int>(kThemeCategoryCount);
}

/// Capability descriptor — which UI controls apply to this category.
struct SettingsCapability final {
    bool foreground : 1;
    bool background : 1;
    bool style      : 1; // bold / italic / underlined
    bool font       : 1; // font face
    bool fontSize   : 1; // font size
    bool separator  : 1; // separator line colour
};

/// Capability per settings category. Default syntax style carries the
/// editor-wide font + size; all other categories hide those controls.
constexpr auto capabilityOf(const SettingsCategory category) -> SettingsCapability {
    switch (category) {
    case SettingsCategory::Default:
        return { .foreground = true, .background = true, .style = true, .font = true, .fontSize = true, .separator = true };
    case SettingsCategory::LineNumber:
    case SettingsCategory::Selection:
    case SettingsCategory::FoldMargin:
        return { .foreground = true, .background = true, .style = false, .font = false, .fontSize = false, .separator = false };
    default:
        // syntax styles (except Default) + Brace/BadBrace: colours + style
        return { .foreground = true, .background = true, .style = true, .font = false, .fontSize = false, .separator = false };
    }
}

} // namespace fbide
