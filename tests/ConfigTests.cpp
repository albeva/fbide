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

static const wxString testIniPath = FBIDE_TEST_DATA_DIR "prefs.ini";

TEST(ConfigTests, Defaults) {
    const Config cfg(wxGetCwd());
    EXPECT_FALSE(cfg.autoIndent());
    EXPECT_EQ(cfg.tabSize(), 4);
    EXPECT_EQ(cfg.language(), "english");
    EXPECT_EQ(cfg.syntaxFile(), "fbfull.lng");
    EXPECT_EQ(cfg.themeFile(), "classic.fbt");
}

TEST(ConfigTests, LoadLegacyIni) {
    Config cfg(wxGetCwd());
    cfg.load(testIniPath);

    EXPECT_TRUE(cfg.autoIndent());
    EXPECT_TRUE(cfg.syntaxHighlight());
    EXPECT_EQ(cfg.tabSize(), 4);
    EXPECT_EQ(cfg.language(), "english");
    EXPECT_EQ(cfg.compilerPath(), "fbc.exe");
    EXPECT_EQ(cfg.windowW(), 902);
}

TEST(ConfigTests, SaveAndReload) {
    const wxString tmpFile = wxFileName::CreateTempFileName("fbide_cfg");
    wxCopyFile(testIniPath, tmpFile, true);

    Config cfg(wxGetCwd());
    cfg.load(tmpFile);
    cfg.save();

    Config cfg2(wxGetCwd());
    cfg2.load(tmpFile);

    EXPECT_EQ(cfg.autoIndent(), cfg2.autoIndent());
    EXPECT_EQ(cfg.tabSize(), cfg2.tabSize());
    EXPECT_EQ(cfg.language(), cfg2.language());
    EXPECT_EQ(cfg.compilerPath(), cfg2.compilerPath());
    EXPECT_EQ(cfg.windowW(), cfg2.windowW());

    wxRemoveFile(tmpFile);
}
