//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "config/Keywords.hpp"
#include <gtest/gtest.h>

using namespace fbide;

class KeywordsTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
};

TEST_F(KeywordsTests, LoadFbFull) {
    Keywords kw;
    kw.load(testDataPath + "fbfull.lng");

    EXPECT_FALSE(kw.getGroup(0).empty());
    EXPECT_FALSE(kw.getGroup(1).empty());
    EXPECT_FALSE(kw.getGroup(2).empty());
    EXPECT_FALSE(kw.getGroup(3).empty());

    // kw1 contains common FB keywords
    EXPECT_NE(kw.getGroup(0).Find("dim"), wxNOT_FOUND);
    EXPECT_NE(kw.getGroup(0).Find("print"), wxNOT_FOUND);

    // sorted list has all words combined
    EXPECT_FALSE(kw.getSortedList().empty());
    EXPECT_NE(kw.getSortedList().Index("dim"), wxNOT_FOUND);
}
