//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/config/Lang.hpp"
#include <gtest/gtest.h>

using namespace fbide;

static const wxString testDataPath = FBIDE_TEST_DATA_DIR "lang/";

TEST(LangTests, LoadEnglish) {
    Lang lang;
    lang.load(testDataPath + "english.fbl");

    EXPECT_EQ(lang[LangId::Welcome], "Welcome to FBIde!");
    EXPECT_EQ(lang[LangId::OK], "&OK");
    EXPECT_EQ(lang[LangId::Cancel], "&Cancel");
    EXPECT_EQ(lang[LangId::MenuFile], "&File");
    EXPECT_EQ(lang[LangId::FileNew], "&New ..");
    EXPECT_EQ(lang[LangId::EditUndo], "&Undo");
    EXPECT_EQ(lang[LangId::RunCompile], "Compile");
    EXPECT_EQ(lang[LangId::ToolbarNew], "New file (Ctrl+N)");
    EXPECT_EQ(lang[LangId::Untitled], "Untitled");
    EXPECT_EQ(lang[LangId::SaveChanges], "Save changes?");
}

TEST(LangTests, LoadGerman) {
    Lang lang;
    lang.load(testDataPath + "deutsch.fbl");

    EXPECT_EQ(lang[LangId::MenuFile], "&Datei");
    EXPECT_FALSE(lang[LangId::EditUndo].empty());
    EXPECT_FALSE(lang[LangId::FileQuit].empty());
}

TEST(LangTests, MissingKeyReturnsEmpty) {
    Lang lang;
    lang.load(testDataPath + "english.fbl");

    // Index 0 is not used in the language files
    EXPECT_EQ(lang.get(static_cast<LangId>(999)), "");
}

TEST(LangTests, Reload) {
    Lang lang;
    lang.load(testDataPath + "english.fbl");
    EXPECT_EQ(lang[LangId::MenuFile], "&File");

    lang.load(testDataPath + "deutsch.fbl");
    EXPECT_EQ(lang[LangId::MenuFile], "&Datei");
}

TEST(LangTests, Clear) {
    Lang lang;
    lang.load(testDataPath + "english.fbl");
    EXPECT_EQ(lang[LangId::Welcome], "Welcome to FBIde!");

    lang.clear();
    EXPECT_EQ(lang[LangId::Welcome], "");
}

TEST(LangTests, NonexistentFile) {
    Lang lang;
    lang.load("/nonexistent/path.fbl");
    EXPECT_EQ(lang[LangId::Welcome], "");
}
