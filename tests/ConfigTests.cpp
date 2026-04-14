//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/config/Config.hpp"
#include <gtest/gtest.h>
#include <wx/filename.h>

using namespace fbide;

class ConfigTests : public testing::Test {
protected:
    static inline const wxString testIniPath = FBIDE_TEST_DATA_DIR "prefs.ini";
};

TEST_F(ConfigTests, Defaults) {
    const Config cfg(wxGetCwd());
    EXPECT_TRUE(cfg.getAutoIndent());
    EXPECT_EQ(cfg.getTabSize(), 4);
    EXPECT_EQ(cfg.getLanguage(), "english");
    EXPECT_EQ(cfg.getSyntaxFile(), "fbfull.lng");
    EXPECT_EQ(cfg.getTheme(), "classic");
}

TEST_F(ConfigTests, LoadLegacyIni) {
    Config cfg(wxGetCwd());
    cfg.load(testIniPath);

    EXPECT_TRUE(cfg.getAutoIndent());
    EXPECT_TRUE(cfg.getSyntaxHighlight());
    EXPECT_EQ(cfg.getTabSize(), 4);
    EXPECT_EQ(cfg.getLanguage(), "english");
    EXPECT_EQ(cfg.getCompilerPath(), "fbc.exe");
    EXPECT_EQ(cfg.getWindowW(), 902);
}

TEST_F(ConfigTests, SaveAndReload) {
    const wxString tmpFile = wxFileName::CreateTempFileName("fbide_cfg");
    wxCopyFile(testIniPath, tmpFile, true);

    Config cfg(wxGetCwd());
    cfg.load(tmpFile);
    cfg.save();

    Config cfg2(wxGetCwd());
    cfg2.load(tmpFile);

    EXPECT_EQ(cfg.getAutoIndent(), cfg2.getAutoIndent());
    EXPECT_EQ(cfg.getTabSize(), cfg2.getTabSize());
    EXPECT_EQ(cfg.getLanguage(), cfg2.getLanguage());
    EXPECT_EQ(cfg.getCompilerPath(), cfg2.getCompilerPath());
    EXPECT_EQ(cfg.getWindowW(), cfg2.getWindowW());

    wxRemoveFile(tmpFile);
}
