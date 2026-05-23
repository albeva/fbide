//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/markdown
//
#include <gtest/gtest.h>
#include "MarkdownTestFixtures.hpp"
#include "markdown/Markdown.hpp"
#include "markdown/MarkdownLayout.hpp"
#include "markdown/MarkdownRenderer.hpp"

using namespace fbide;
using namespace fbide::tests;

namespace {

constexpr int kTestWidth = 500;

auto layout(const wxString& markdown) -> LaidOutDoc {
    const FakeMeasurer measurer;
    return layoutMarkdown(parseMarkdown(markdown), kTestWidth, measurer, fakePalette(), splitHighlight);
}

/// Find a run on a given line that contains `needle` and return its index.
auto findRun(const PaintLine& line, const wxString& needle) -> std::size_t {
    for (std::size_t r = 0; r < line.runs.size(); r++) {
        if (line.runs.at(r).text.Contains(needle)) {
            return r;
        }
    }
    return line.runs.size();
}

} // namespace

class MarkdownSelectionTests : public testing::Test {}; // NOLINT(misc-use-internal-linkage)

// ---------------------------------------------------------------------------
// SelectionPosition / Selection
// ---------------------------------------------------------------------------

TEST_F(MarkdownSelectionTests, EmptySelectionByDefault) {
    const Selection sel;
    EXPECT_TRUE(sel.empty());
}

TEST_F(MarkdownSelectionTests, RangeNormalisesBackwardsDrag) {
    Selection sel;
    sel.anchor = { 2, 0, 0 };
    sel.caret = { 1, 0, 0 };
    const auto [start, end] = sel.range();
    EXPECT_EQ(start.lineIndex, 1U);
    EXPECT_EQ(end.lineIndex, 2U);
}

TEST_F(MarkdownSelectionTests, ClearResetsToEmpty) {
    Selection sel;
    sel.anchor = { 0, 0, 3 };
    sel.caret = { 0, 0, 8 };
    EXPECT_FALSE(sel.empty());
    sel.clear();
    EXPECT_TRUE(sel.empty());
}

// ---------------------------------------------------------------------------
// hitTestLine
// ---------------------------------------------------------------------------

TEST_F(MarkdownSelectionTests, HitTestLandsAtCorrectCharacter) {
    const auto doc = layout("hello world");
    ASSERT_FALSE(doc.lines.empty());
    const auto& line = doc.lines.at(0);
    const FakeMeasurer measurer;
    // FakeMeasurer is 10 px / prose char. "hello" starts at x=0. Clicking
    // at x=25 inside the first run lands between 'h' and 'e' / 'l' — at
    // char index 2 (or 3 depending on round); binary search snaps to the
    // largest prefix whose width is <= the target, so 25 px → 2 chars.
    const auto [runIdx, charIdx] = hitTestLine(line, 25, measurer);
    EXPECT_EQ(runIdx, 0U);
    EXPECT_EQ(charIdx, 2U);
}

TEST_F(MarkdownSelectionTests, HitTestClampsLeftEdge) {
    const auto doc = layout("hello");
    const FakeMeasurer measurer;
    const auto [runIdx, charIdx] = hitTestLine(doc.lines.at(0), -10, measurer);
    EXPECT_EQ(runIdx, 0U);
    EXPECT_EQ(charIdx, 0U);
}

TEST_F(MarkdownSelectionTests, HitTestClampsRightEdge) {
    const auto doc = layout("hello");
    const FakeMeasurer measurer;
    const auto [runIdx, charIdx] = hitTestLine(doc.lines.at(0), 99999, measurer);
    EXPECT_EQ(runIdx, 0U);
    EXPECT_EQ(charIdx, 5U); // past the last char
}

// ---------------------------------------------------------------------------
// extractSelectedText
// ---------------------------------------------------------------------------

TEST_F(MarkdownSelectionTests, ExtractEmptySelectionYieldsEmpty) {
    const auto doc = layout("hello");
    const Selection sel;
    EXPECT_TRUE(extractSelectedText(doc, sel).empty());
}

TEST_F(MarkdownSelectionTests, ExtractWholeFirstRun) {
    const auto doc = layout("hello world");
    Selection sel;
    sel.anchor = { 0, 0, 0 };
    sel.caret = { 0, 0, 5 }; // "hello"
    EXPECT_EQ(extractSelectedText(doc, sel), "hello");
}

TEST_F(MarkdownSelectionTests, ExtractAcrossRunsOnSameLine) {
    // "hello **bold** world" → ["hello", " ", "bold", " ", "world"] (or
    // similar). Selecting from start of "hello" through end of "bold".
    const auto doc = layout("hello **bold** world");
    const auto& line = doc.lines.at(0);
    const std::size_t boldRun = findRun(line, "bold");
    ASSERT_LT(boldRun, line.runs.size());

    Selection sel;
    sel.anchor = { 0, 0, 0 };
    sel.caret = { 0, boldRun, line.runs.at(boldRun).text.length() };
    const wxString text = extractSelectedText(doc, sel);
    EXPECT_TRUE(text.Contains("hello"));
    EXPECT_TRUE(text.Contains("bold"));
    EXPECT_FALSE(text.Contains("world"));
}

TEST_F(MarkdownSelectionTests, ExtractAcrossLinesJoinsWithNewline) {
    const auto doc = layout("first paragraph\n\nsecond paragraph");
    ASSERT_GE(doc.lines.size(), 2U);

    Selection sel;
    sel.anchor = { 0, 0, 0 };
    sel.caret = { 1, 0, doc.lines.at(1).runs.at(0).text.length() };
    const wxString text = extractSelectedText(doc, sel);
    EXPECT_TRUE(text.Contains("first"));
    EXPECT_TRUE(text.Contains("second"));
    EXPECT_TRUE(text.Contains("\n"));
}

TEST_F(MarkdownSelectionTests, ExtractIsNormalisedAcrossBackwardsDrag) {
    const auto doc = layout("hello");
    Selection sel;
    sel.anchor = { 0, 0, 5 };
    sel.caret = { 0, 0, 0 };
    EXPECT_EQ(extractSelectedText(doc, sel), "hello");
}
