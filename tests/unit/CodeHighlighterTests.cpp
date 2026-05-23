//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/chat/CodeHighlighter.hpp"
#include "config/Theme.hpp"

using namespace fbide;
using namespace fbide::ai;
using lexer::Token;
using lexer::TokenKind;

namespace {

// Distinct, easily-recognised colours so a run's colour pins down which
// theme category it was resolved through.
const wxColour kDefaultFg { 10, 10, 10 };
const wxColour kKeywordFg { 20, 120, 220 };
const wxColour kCommentFg { 0, 160, 0 };

/// A theme with just the categories the tests exercise populated.
auto makeTheme() -> Theme {
    Theme theme;
    theme.set(ThemeCategory::Default, Theme::Entry { .colors = { .foreground = kDefaultFg } });
    theme.set(ThemeCategory::Keywords,
        Theme::Entry { .colors = { .foreground = kKeywordFg }, .bold = true });
    theme.set(ThemeCategory::Comment,
        Theme::Entry { .colors = { .foreground = kCommentFg }, .italic = true });
    return theme;
}

/// Build a token of `kind` carrying `text`.
auto tok(const TokenKind kind, const std::string& text) -> Token {
    Token token;
    token.kind = kind;
    token.text = text;
    return token;
}

} // namespace

class CodeHighlighterTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Line splitting
// ---------------------------------------------------------------------------

TEST_F(CodeHighlighterTests, EmptyInputYieldsOneEmptyLine) {
    const auto lines = highlightCode({}, makeTheme());
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_TRUE(lines[0].empty());
}

TEST_F(CodeHighlighterTests, SingleRunOneLine) {
    const auto lines = highlightCode({ tok(TokenKind::Identifier, "foo") }, makeTheme());
    ASSERT_EQ(lines.size(), 1U);
    ASSERT_EQ(lines[0].size(), 1U);
    EXPECT_EQ(lines[0][0].text, "foo");
}

TEST_F(CodeHighlighterTests, NewlineTokenSplitsLines) {
    const std::vector<Token> tokens {
        tok(TokenKind::Identifier, "a"),
        tok(TokenKind::Newline, "\n"),
        tok(TokenKind::Identifier, "b"),
    };
    const auto lines = highlightCode(tokens, makeTheme());
    ASSERT_EQ(lines.size(), 2U);
    ASSERT_EQ(lines[0].size(), 1U);
    EXPECT_EQ(lines[0][0].text, "a");
    ASSERT_EQ(lines[1].size(), 1U);
    EXPECT_EQ(lines[1][0].text, "b");
}

TEST_F(CodeHighlighterTests, TrailingNewlineIsDropped) {
    // md4c '\n'-terminates fenced code — the spurious blank line must go.
    const std::vector<Token> tokens {
        tok(TokenKind::Identifier, "a"),
        tok(TokenKind::Newline, "\n"),
    };
    const auto lines = highlightCode(tokens, makeTheme());
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines[0][0].text, "a");
}

TEST_F(CodeHighlighterTests, BlankLineInMiddleIsKept) {
    const std::vector<Token> tokens {
        tok(TokenKind::Identifier, "a"),
        tok(TokenKind::Newline, "\n"),
        tok(TokenKind::Newline, "\n"),
        tok(TokenKind::Identifier, "b"),
    };
    const auto lines = highlightCode(tokens, makeTheme());
    ASSERT_EQ(lines.size(), 3U);
    EXPECT_TRUE(lines[1].empty());
}

TEST_F(CodeHighlighterTests, MultilineTokenTextIsSplit) {
    // A `/' ... '/` block comment is one token whose text spans lines.
    const auto lines = highlightCode({ tok(TokenKind::CommentBlock, "/'one\ntwo'/") }, makeTheme());
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0][0].text, "/'one");
    EXPECT_EQ(lines[1][0].text, "two'/");
}

// ---------------------------------------------------------------------------
// Colour mapping
// ---------------------------------------------------------------------------

TEST_F(CodeHighlighterTests, KeywordRunGetsKeywordColour) {
    const auto lines = highlightCode({ tok(TokenKind::Keywords, "Print") }, makeTheme());
    ASSERT_EQ(lines[0].size(), 1U);
    EXPECT_EQ(lines[0][0].colour, kKeywordFg);
    EXPECT_TRUE(lines[0][0].bold);
    EXPECT_FALSE(lines[0][0].italic);
}

TEST_F(CodeHighlighterTests, CommentRunGetsCommentColour) {
    const auto lines = highlightCode({ tok(TokenKind::Comment, "' note") }, makeTheme());
    EXPECT_EQ(lines[0][0].colour, kCommentFg);
    EXPECT_TRUE(lines[0][0].italic);
}

TEST_F(CodeHighlighterTests, IdentifierUsesDefaultColour) {
    const auto lines = highlightCode({ tok(TokenKind::Identifier, "myVar") }, makeTheme());
    EXPECT_EQ(lines[0][0].colour, kDefaultFg);
    EXPECT_FALSE(lines[0][0].bold);
}

TEST_F(CodeHighlighterTests, WhitespaceUsesDefaultColour) {
    const auto lines = highlightCode({ tok(TokenKind::Whitespace, "    ") }, makeTheme());
    ASSERT_EQ(lines[0].size(), 1U);
    EXPECT_EQ(lines[0][0].colour, kDefaultFg);
}

// ---------------------------------------------------------------------------
// Tab expansion
// ---------------------------------------------------------------------------

TEST_F(CodeHighlighterTests, LeadingTabExpandsToSpaces) {
    const std::vector<Token> tokens {
        tok(TokenKind::Whitespace, "\t"),
        tok(TokenKind::Identifier, "x"),
    };
    const auto lines = highlightCode(tokens, makeTheme(), 4);
    ASSERT_EQ(lines.size(), 1U);
    ASSERT_EQ(lines[0].size(), 2U);
    EXPECT_EQ(lines[0][0].text, "    "); // tab at column 0 → 4 spaces
    EXPECT_EQ(lines[0][1].text, "x");
}

TEST_F(CodeHighlighterTests, TabAdvancesToNextTabStop) {
    // A tab from column 2 (tab width 4) fills just 2 spaces.
    const auto lines = highlightCode({ tok(TokenKind::Identifier, "ab\tc") }, makeTheme(), 4);
    ASSERT_EQ(lines[0].size(), 1U);
    EXPECT_EQ(lines[0][0].text, "ab  c");
}

TEST_F(CodeHighlighterTests, TabColumnResetsEachLine) {
    const std::vector<Token> tokens {
        tok(TokenKind::Identifier, "abc"),
        tok(TokenKind::Newline, "\n"),
        tok(TokenKind::Whitespace, "\t"),
        tok(TokenKind::Identifier, "y"),
    };
    const auto lines = highlightCode(tokens, makeTheme(), 4);
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[1][0].text, "    "); // tab on a fresh line → full 4 spaces
}

// ---------------------------------------------------------------------------
// A small mixed line
// ---------------------------------------------------------------------------

TEST_F(CodeHighlighterTests, MixedLineKeepsRunOrderAndColours) {
    const std::vector<Token> tokens {
        tok(TokenKind::Keywords, "Print"),
        tok(TokenKind::Whitespace, " "),
        tok(TokenKind::Identifier, "x"),
    };
    const auto lines = highlightCode(tokens, makeTheme());
    ASSERT_EQ(lines.size(), 1U);
    ASSERT_EQ(lines[0].size(), 3U);
    EXPECT_EQ(lines[0][0].text, "Print");
    EXPECT_EQ(lines[0][0].colour, kKeywordFg);
    EXPECT_EQ(lines[0][1].text, " ");
    EXPECT_EQ(lines[0][2].text, "x");
    EXPECT_EQ(lines[0][2].colour, kDefaultFg);
}
