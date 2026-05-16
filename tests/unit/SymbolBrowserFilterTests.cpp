//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "sidebar/SymbolBrowser.hpp"

using namespace fbide;

TEST(SymbolBrowserFilterTests, ParseBlankYieldsNoWords) {
    EXPECT_TRUE(parseSymbolFilter("").empty());
    EXPECT_TRUE(parseSymbolFilter("   \t  ").empty());
}

TEST(SymbolBrowserFilterTests, ParseLowercasesAndSplitsOnWhitespace) {
    const auto words = parseSymbolFilter("  Foo\tBAR  Baz ");
    ASSERT_EQ(words.size(), 3U);
    EXPECT_EQ(words[0], "foo");
    EXPECT_EQ(words[1], "bar");
    EXPECT_EQ(words[2], "baz");
}

TEST(SymbolBrowserFilterTests, EmptyWordListMatchesAnything) {
    EXPECT_TRUE(symbolFilterMatches({}, "anything at all"));
    EXPECT_TRUE(symbolFilterMatches({}, ""));
}

TEST(SymbolBrowserFilterTests, EveryWordMustMatch) {
    const std::vector<wxString> words { "foo", "bar" };
    EXPECT_TRUE(symbolFilterMatches(words, "foo and bar present"));
    EXPECT_FALSE(symbolFilterMatches(words, "only foo present"));
}

TEST(SymbolBrowserFilterTests, WordsMatchAsSubstrings) {
    // A single word matches anywhere in the haystack — name, kind keyword
    // or owner-qualified prefix all live in the same lowercased string.
    EXPECT_TRUE(symbolFilterMatches({ "ype" }, "mytype.draw function"));
    EXPECT_TRUE(symbolFilterMatches({ "mytype" }, "mytype.draw function"));
    EXPECT_FALSE(symbolFilterMatches({ "missing" }, "mytype.draw function"));
}
