//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "compiler/FbcDefines.hpp"

using namespace fbide;

TEST(FbcDefinesTests, ParsesMarkedLinesLowercased) {
    wxArrayString lines;
    lines.Add("FBDEF __FB_UNIX__");
    lines.Add("FBDEF __FB_DARWIN__");
    lines.Add("some unrelated compiler note");
    lines.Add("FBDEF __FB_64BIT__");

    const auto defs = parseFbcDefines(lines);
    EXPECT_EQ(defs.size(), 3U);
    EXPECT_TRUE(defs.contains("__fb_unix__"));
    EXPECT_TRUE(defs.contains("__fb_darwin__"));
    EXPECT_TRUE(defs.contains("__fb_64bit__"));
    EXPECT_FALSE(defs.contains("__fb_win32__"));
}

TEST(FbcDefinesTests, IgnoresLocationPrefixAndTrailingTokens) {
    wxArrayString lines;
    lines.Add("/path/fbc-defines.bas(12): FBDEF __FB_ARM__   "); // location prefix + trailing ws
    lines.Add("   FBDEF   __Fb_Linux__\textra");                 // leading ws, mixed case, extra token

    const auto defs = parseFbcDefines(lines);
    EXPECT_EQ(defs.size(), 2U);
    EXPECT_TRUE(defs.contains("__fb_arm__"));
    EXPECT_TRUE(defs.contains("__fb_linux__"));
}

TEST(FbcDefinesTests, EmptyWhenNoMarker) {
    wxArrayString lines;
    lines.Add("compiling fbc-defines.bas");
    lines.Add("");
    EXPECT_TRUE(parseFbcDefines(lines).empty());
}
