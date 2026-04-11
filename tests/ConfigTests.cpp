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
    Config cfg("/usr/local/bin/fbide");
    EXPECT_FALSE(cfg.autoIndent);
    EXPECT_FALSE(cfg.syntaxHighlight);
    EXPECT_EQ(cfg.tabSize, 4);
    EXPECT_EQ(cfg.edgeColumn, 80);
    EXPECT_EQ(cfg.language, "english");
    EXPECT_TRUE(cfg.activePath);
    EXPECT_EQ(cfg.syntaxFile, "fbfull.lng");
    EXPECT_EQ(cfg.themeFile, "classic");
    EXPECT_EQ(cfg.windowX, 50);
    EXPECT_EQ(cfg.windowW, 350);
}

TEST(ConfigTests, BinaryPath) {
    Config cfg("/usr/local/bin/fbide");
    EXPECT_EQ(cfg.getBinaryPath(), "/usr/local/bin/");
    EXPECT_EQ(cfg.getIdePath(), "/usr/local/bin/IDE/");
}

TEST(ConfigTests, SetIdePath) {
    Config cfg("/usr/local/bin/fbide");
    cfg.setIdePath("/opt/fbide/config");
    EXPECT_EQ(cfg.getIdePath(), "/opt/fbide/config/");
}

TEST(ConfigTests, LoadLegacyIni) {
    Config cfg("/usr/local/bin/fbide");
    cfg.load(testIniPath);

    // [general]
    EXPECT_TRUE(cfg.autoIndent);
    EXPECT_TRUE(cfg.syntaxHighlight);
    EXPECT_TRUE(cfg.longLine);
    EXPECT_FALSE(cfg.whiteSpace);
    EXPECT_TRUE(cfg.lineNumbers);
    EXPECT_FALSE(cfg.indentGuide);
    EXPECT_TRUE(cfg.braceHighlight);
    EXPECT_TRUE(cfg.showExitCode);
    EXPECT_FALSE(cfg.folderMargin);
    EXPECT_FALSE(cfg.displayEOL);
    EXPECT_FALSE(cfg.currentLine);
    EXPECT_TRUE(cfg.activePath);
    EXPECT_EQ(cfg.tabSize, 4);
    EXPECT_EQ(cfg.edgeColumn, 80);
    EXPECT_EQ(cfg.language, "english");

    // [paths]
    EXPECT_EQ(cfg.compilerPath, "fbc.exe");
    EXPECT_EQ(cfg.syntaxFile, "fbfull.lng");
    EXPECT_EQ(cfg.themeFile, "classic");

    // [compiler]
    EXPECT_FALSE(cfg.compileCommand.empty());
    EXPECT_FALSE(cfg.runCommand.empty());

    // [editor]
    EXPECT_TRUE(cfg.floatBars);
    EXPECT_TRUE(cfg.splashScreen);
    EXPECT_EQ(cfg.windowX, 540);
    EXPECT_EQ(cfg.windowY, 293);
    EXPECT_EQ(cfg.windowW, 902);
    EXPECT_EQ(cfg.windowH, 787);
}

TEST(ConfigTests, Reset) {
    Config cfg("/usr/local/bin/fbide");
    cfg.load(testIniPath);
    EXPECT_TRUE(cfg.autoIndent);

    cfg.reset();
    EXPECT_FALSE(cfg.autoIndent);
    EXPECT_EQ(cfg.tabSize, 4);
    // Paths preserved after reset
    EXPECT_EQ(cfg.getBinaryPath(), "/usr/local/bin/");
}

TEST(ConfigTests, SaveAndReload) {
    Config cfg("/usr/local/bin/fbide");
    cfg.load(testIniPath);

    wxString tmpFile = wxFileName::CreateTempFileName("fbide_cfg");
    cfg.save(tmpFile);

    Config cfg2("/usr/local/bin/fbide");
    cfg2.load(tmpFile);

    EXPECT_EQ(cfg.autoIndent, cfg2.autoIndent);
    EXPECT_EQ(cfg.syntaxHighlight, cfg2.syntaxHighlight);
    EXPECT_EQ(cfg.tabSize, cfg2.tabSize);
    EXPECT_EQ(cfg.edgeColumn, cfg2.edgeColumn);
    EXPECT_EQ(cfg.language, cfg2.language);
    EXPECT_EQ(cfg.compilerPath, cfg2.compilerPath);
    EXPECT_EQ(cfg.windowX, cfg2.windowX);
    EXPECT_EQ(cfg.windowH, cfg2.windowH);
    EXPECT_EQ(cfg.splashScreen, cfg2.splashScreen);

    wxRemoveFile(tmpFile);
}

TEST(ConfigTests, NonexistentFile) {
    Config cfg("/usr/local/bin/fbide");
    cfg.load("/nonexistent/prefs.ini");
    // Should keep defaults
    EXPECT_FALSE(cfg.autoIndent);
    EXPECT_EQ(cfg.tabSize, 4);
}
