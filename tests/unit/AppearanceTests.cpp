//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "app/Appearance.hpp"

using namespace fbide;
using Appearance = wxApp::Appearance;

TEST(AppearanceTests, ParsesConfigValues) {
    EXPECT_EQ(parseAppearance("system"), Appearance::System);
    EXPECT_EQ(parseAppearance("dark"), Appearance::Dark);
    EXPECT_EQ(parseAppearance("light"), Appearance::Light);
    EXPECT_EQ(parseAppearance("Dark"), Appearance::Dark); // case-insensitive
}

TEST(AppearanceTests, EmptyAndUnknownValuesDoNotParse) {
    // Empty means "leave the platform default alone"; unknown is rejected.
    EXPECT_FALSE(parseAppearance("").has_value());
    EXPECT_FALSE(parseAppearance("sepia").has_value());
}

TEST(AppearanceTests, ConfigKeyRoundTrips) {
    for (const auto appearance : { Appearance::System, Appearance::Dark, Appearance::Light }) {
        EXPECT_EQ(parseAppearance(appearanceConfigKey(appearance)), appearance);
    }
}

TEST(AppearanceTests, SupportMatchesPlatform) {
#if defined(__WXOSX__)
    // The test hosts run macOS 10.14+, where wx can switch appearance.
    EXPECT_TRUE(isAppearanceSupported());
#elif defined(__WXGTK3__)
    EXPECT_TRUE(isAppearanceSupported());
#elif defined(__WXMSW__)
    EXPECT_EQ(isAppearanceSupported(), wxCheckOsVersion(10, 0, 18362));
#else
    EXPECT_FALSE(isAppearanceSupported());
#endif
}
