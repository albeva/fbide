//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "config/Theme.hpp"
#include <gtest/gtest.h>
#include <wx/filename.h>

using namespace fbide;

class ThemeTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
};

TEST_F(ThemeTests, LoadClassic) {
    Theme theme;
    theme.load(testDataPath + "classic.fbt");

    EXPECT_EQ(theme.getDefault().background, *wxWHITE);
    EXPECT_EQ(theme.getDefault().foreground, *wxBLACK);
    EXPECT_EQ(theme.getDefault().fontSize, 12);
    EXPECT_TRUE(theme.getStyle(Theme::Keyword).fontStyle.bold);
}

TEST_F(ThemeTests, LoadObsidian) {
    Theme theme;
    theme.load(testDataPath + "obsidian.fbt");

    EXPECT_NE(theme.getDefault().background, *wxWHITE);
}

TEST_F(ThemeTests, SaveAndReload) {
    const wxString tmpFile = wxFileName::CreateTempFileName("fbide_theme");

    Theme theme;
    theme.load(testDataPath + "classic.fbt");
    theme.setPath(tmpFile);
    theme.save();

    Theme theme2;
    theme2.load(tmpFile);
    EXPECT_EQ(theme2.getDefault().background, *wxWHITE);
    EXPECT_EQ(theme2.getDefault().foreground, *wxBLACK);
    EXPECT_EQ(theme2.getDefault().fontSize, 12);
    EXPECT_TRUE(theme2.getStyle(Theme::Keyword).fontStyle.bold);

    wxRemoveFile(tmpFile);
}
