//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
// clang-format off
namespace fbide {

// ===========================================================================
// Keyword groups
// ===========================================================================

#define DEFINE_THEME_KEYWORD_GROUPS(_) \
    _( Keywords         ) \
    _( KeywordTypes     ) \
    _( KeywordOperators ) \
    _( KeywordConstants ) \
    _( KeywordLibrary   ) \
    _( KeywordCustom    ) \
    _( KeywordPP        ) \
    _( KeywordAsm1      ) \
    _( KeywordAsm2      ) \

/// Syntax style categories. These map 1:1 to Scintilla style IDs in
/// the FBStc lexer and per-style entries in Theme.
#define DEFINE_THEME_CATEGORY(_)   \
    _(Default)                     \
    _(Comment)                     \
    _(MultilineComment)            \
    _(Number)                      \
    _(String)                      \
    _(StringOpen)                  \
    _(Identifier)                  \
    DEFINE_THEME_KEYWORD_GROUPS(_) \
    _(Operator)                    \
    _(Label)                       \
    _(Preprocessor)                \
    _(Error)

enum class ThemeCategory : int {
    #define ENUM(NAME) NAME,
        DEFINE_THEME_CATEGORY(ENUM)
    #undef ENUM
};

/// Allow simple conversion from enum to int
/// int labelId = +ThemeCategory::Label;
constexpr auto operator+(const ThemeCategory& rhs) -> int {
    return static_cast<int>(rhs);
}

/// All syntax categories enumerated
inline constexpr std::array kThemeCategories {
    #define ALL(NAME) ThemeCategory::NAME,
        DEFINE_THEME_CATEGORY(ALL)
    #undef ALL
};

/// Total number of theme categories
inline constexpr std::size_t kThemeCategoryCount = kThemeCategories.size();

/// Get syntax category name
constexpr auto getThemeCategoryName(const ThemeCategory category) -> std::string_view {
    switch (category) {
        #define CASE(NAME) case ThemeCategory::NAME: return #NAME;
            DEFINE_THEME_CATEGORY(CASE)
        #undef CASE
    }
    std::unreachable();
}

/// Keyword groups
inline constexpr std::array kThemeKeywordCategories {
    #define GROUPS(NAME) ThemeCategory::NAME,
        DEFINE_THEME_KEYWORD_GROUPS(GROUPS)
    #undef GROUPS
};
inline constexpr std::size_t kThemeKeywordGroupsCount = kThemeKeywordCategories.size();

static constexpr auto indexOfKeywordGroup(const ThemeCategory cat) -> std::size_t {
    const auto it = std::ranges::find(kThemeKeywordCategories, cat);
    if (it == kThemeKeywordCategories.end()) {
        std::unreachable();
    }
    return static_cast<std::size_t>(std::ranges::distance(kThemeKeywordCategories.begin(), it));
};

/// True when `cat` is one of the keyword-group categories
/// (Keyword1..Keyword4, KeywordLibrary/2, KeywordPP, KeywordAsm1/2).
constexpr auto isKeywordCategory(const ThemeCategory cat) -> bool {
    return std::ranges::find(kThemeKeywordCategories, cat) != kThemeKeywordCategories.end();
}


} // namespace fbide
