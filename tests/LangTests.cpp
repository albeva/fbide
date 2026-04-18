//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "config/Lang.hpp"
#include <gtest/gtest.h>

using namespace fbide;

class LangTests : public testing::Test {
protected:
    static inline const wxString testLangPath = FBIDE_TEST_DATA_DIR "lang/";
};

TEST_F(LangTests, LoadEnglish) {
    Lang lang;
    lang.load(testLangPath + "english.fbl");

    EXPECT_EQ(lang[LangId::Welcome], "Welcome to FBIde!");
    EXPECT_EQ(lang[LangId::MenuFile], "&File");
    EXPECT_EQ(lang[LangId::EditUndo], "&Undo");
    EXPECT_EQ(lang[LangId::Untitled], "Untitled");
}

TEST_F(LangTests, Reload) {
    Lang lang;
    lang.load(testLangPath + "english.fbl");
    EXPECT_EQ(lang[LangId::MenuFile], "&File");

    lang.load(testLangPath + "deutsch.fbl");
    EXPECT_EQ(lang[LangId::MenuFile], "&Datei");
}

TEST_F(LangTests, MissingKey) {
    Lang lang;
    lang.load(testLangPath + "english.fbl");
    EXPECT_EQ(lang.get(static_cast<LangId>(999)), "");
}
