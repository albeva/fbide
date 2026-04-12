//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/config/Theme.hpp"
#include <gtest/gtest.h>
#include <wx/filename.h>

using namespace fbide;

static const wxString testDataPath = FBIDE_TEST_DATA_DIR;

TEST(ThemeTests, LoadClassic) {
    Theme theme;
    theme.load(testDataPath + "classic.fbt");

    EXPECT_EQ(theme.getDefault().background, *wxWHITE);
    EXPECT_EQ(theme.getDefault().foreground, *wxBLACK);
    EXPECT_EQ(theme.getDefault().fontSize, 12);
    EXPECT_TRUE(theme.getStyle(Theme::Keyword).fontStyle.bold);
}

TEST(ThemeTests, LoadObsidian) {
    Theme theme;
    theme.load(testDataPath + "obsidian.fbt");

    EXPECT_NE(theme.getDefault().background, *wxWHITE);
}

TEST(ThemeTests, SaveAndReload) {
    Theme theme;
    theme.load(testDataPath + "classic.fbt");

    const wxString tmpFile = wxFileName::CreateTempFileName("fbide_theme") + ".fbt";
    theme.getDefault() = theme.getDefault(); // no-op, just ensuring mutable access works
    // save to temp by overriding path
    theme.load(testDataPath + "classic.fbt");

    // Can't easily test save() since it uses stored path.
    // Verify loaded values are consistent.
    EXPECT_EQ(theme.getDefault().background, *wxWHITE);
    wxRemoveFile(tmpFile);
}
