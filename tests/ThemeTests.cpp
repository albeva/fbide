//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"
#include "config/Version.hpp"

using namespace fbide;

class ThemeTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
};

TEST_F(ThemeTests, LoadV4Classic) {
    Theme theme;
    theme.loadV4(testDataPath + "classic.fbt");

    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.background, *wxWHITE);
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(theme.getFontSize(), 12);
    EXPECT_TRUE(theme.get(ThemeCategory::Keywords).bold);
    EXPECT_EQ(theme.getVersion(), Version::oldFbide());
}

TEST_F(ThemeTests, LoadV4Obsidian) {
    Theme theme;
    theme.loadV4(testDataPath + "obsidian.fbt");

    EXPECT_NE(theme.get(ThemeCategory::Default).colors.background, *wxWHITE);
}

TEST_F(ThemeTests, LoadDispatchDetectsFbt) {
    Theme theme;
    theme.load(testDataPath + "classic.fbt");

    // Dispatching via extension populates the same fields as loadV4.
    EXPECT_EQ(theme.getFontSize(), 12);
    EXPECT_TRUE(theme.get(ThemeCategory::Keywords).bold);
    EXPECT_EQ(theme.getVersion(), Version::oldFbide());
}

TEST_F(ThemeTests, SaveAndReload) {
    const wxString tmpFile = wxFileName::CreateTempFileName("fbide_theme") + ".ini";

    Theme theme;
    theme.loadV4(testDataPath + "classic.fbt");
    theme.save(tmpFile);

    Theme reloaded;
    reloaded.load(tmpFile);
    EXPECT_EQ(reloaded.get(ThemeCategory::Default).colors.background, *wxWHITE);
    EXPECT_EQ(reloaded.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(reloaded.getFontSize(), 12);
    EXPECT_TRUE(reloaded.get(ThemeCategory::Keywords).bold);
    // Save stamps current version on write.
    EXPECT_EQ(reloaded.getVersion(), Version::fbide());

    wxRemoveFile(tmpFile);
}

TEST_F(ThemeTests, SaveMigratesFbtToIni) {
    // Copy classic.fbt somewhere writable so we can save it.
    const wxString srcFbt = testDataPath + "classic.fbt";
    const wxString dstFbt = wxFileName::CreateTempFileName("fbide_theme") + ".fbt";
    ASSERT_TRUE(wxCopyFile(srcFbt, dstFbt));

    Theme theme;
    theme.load(dstFbt);
    ASSERT_EQ(theme.getVersion(), Version::oldFbide());

    theme.save();

    // Path should have been migrated to .ini.
    wxFileName migrated(theme.getPath());
    EXPECT_EQ(migrated.GetExt().Lower(), "ini");
    EXPECT_TRUE(wxFileExists(migrated.GetFullPath()));

    wxRemoveFile(dstFbt);
    wxRemoveFile(migrated.GetFullPath());
}
