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

// ---------------------------------------------------------------------------
// Declare + definition collapse into one browser entry
// ---------------------------------------------------------------------------

namespace {

auto sub(const char* name, const int line, const bool declaration) -> Symbol {
    return Symbol { .kind = SymbolKind::Sub, .name = name, .owner = {}, .line = line, .declaration = declaration };
}

} // namespace

TEST(SymbolBrowserFilterTests, DeclarationWithDefinitionIsRedundant) {
    // Forum report: `Declare Sub test1()` + `Sub test1 … End Sub` listed twice.
    const std::vector bucket { sub("test1", 0, true), sub("test", 3, false), sub("test1", 12, false) };
    const auto defined = definedSymbolNames(bucket);
    EXPECT_TRUE(isRedundantDeclaration(bucket[0], defined));
    EXPECT_FALSE(isRedundantDeclaration(bucket[1], defined));
    EXPECT_FALSE(isRedundantDeclaration(bucket[2], defined)); // the definition stays
}

TEST(SymbolBrowserFilterTests, DeclarationMatchesDefinitionIgnoringCase) {
    // FreeBASIC names are case-insensitive.
    const std::vector bucket { sub("TEST1", 0, true), sub("test1", 5, false) };
    EXPECT_TRUE(isRedundantDeclaration(bucket[0], definedSymbolNames(bucket)));
}

TEST(SymbolBrowserFilterTests, DeclarationWithoutDefinitionIsKept) {
    // A header-style forward declaration is the procedure's only entry.
    const std::vector bucket { sub("external", 0, true), sub("other", 2, false) };
    EXPECT_FALSE(isRedundantDeclaration(bucket[0], definedSymbolNames(bucket)));
}

TEST(SymbolBrowserFilterTests, OverloadedDefinitionsAreAllKept) {
    // Two definitions of one name are overloads, not duplicates.
    const std::vector bucket { sub("f", 0, true), sub("f", 3, false), sub("f", 7, false) };
    const auto defined = definedSymbolNames(bucket);
    EXPECT_TRUE(isRedundantDeclaration(bucket[0], defined));
    EXPECT_FALSE(isRedundantDeclaration(bucket[1], defined));
    EXPECT_FALSE(isRedundantDeclaration(bucket[2], defined));
}
