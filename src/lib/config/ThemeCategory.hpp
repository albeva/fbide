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

/// Define theme category entries. Expan via passed in macro parameter
/// These categories are used by theme configuratation, editor and FBStc lexer
#define DEFINE_THEME_CATEGORY(_) \
    _(Default)                   \
    _(Comment)                   \
    _(MultilineComment)          \
    _(Number)                    \
    _(String)                    \
    _(StringOpen)                \
    _(Identifier)                \
    _(Keyword1)                  \
    _(Keyword2)                  \
    _(Keyword3)                  \
    _(Keyword4)                  \
    _(Keyword5)                  \
    _(Operator)                  \
    _(Label)                     \
    _(Constant)                  \
    _(Preprocessor)              \
    _(Error)

enum class ThemeCategory : int {
    #define ENUM(NAME) NAME,
        DEFINE_THEME_CATEGORY(ENUM)
    #undef ENUM
};

/// Allow simple conversion from enum to int
/// int labelId = +FBSciLexerState::Label;
constexpr auto operator+(const ThemeCategory& rhs) -> int {
    return static_cast<int>(rhs);
}

/// All categories enumerated
inline constexpr std::array kThemeCategories {
    #define ALL(NAME) ThemeCategory::NAME,
        DEFINE_THEME_CATEGORY(ALL)
    #undef ALL
};

/// Total number of theme categories
inline constexpr std::size_t kThemeCategoryCount = kThemeCategories.size();

/// Get category name
constexpr auto getThemeCategoryName(const ThemeCategory category) -> std::string_view {
    switch (category) {
        #define CASE(NAME) case ThemeCategory::NAME: return #NAME;
            DEFINE_THEME_CATEGORY(CASE)
        #undef CASE
    }
    std::unreachable();
}
} // namespace fbide
