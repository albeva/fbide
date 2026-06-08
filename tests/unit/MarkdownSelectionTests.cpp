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
using namespace fbide::markdown;
using namespace fbide::tests;

namespace {

constexpr int kTestWidth = 500;

auto layout(const wxString& markdown) -> LaidOutDoc {
    const FakeMeasurer measurer;
    return layoutMarkdown(parseMarkdown(markdown), kTestWidth, measurer, fakePalette(), splitHighlight);
}

/// Find a run on a given line that contains `needle` and return its index.
auto findRun(const PaintLine& line, const wxString& needle) -> std::size_t {
    for (std::size_t runIdx = 0; runIdx < line.runs.size(); runIdx++) {
        if (line.runs.at(runIdx).text.Contains(needle)) {
            return runIdx;
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
    sel.anchor = { .lineIndex = 2, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 1, .runIndex = 0, .charInRun = 0 };
    const auto [start, end] = sel.range();
    EXPECT_EQ(start.lineIndex, 1U);
    EXPECT_EQ(end.lineIndex, 2U);
}

TEST_F(MarkdownSelectionTests, ClearResetsToEmpty) {
    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 0, .runIndex = 0, .charInRun = 1 };
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
    const wxString firstWord = "hello";
    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 0, .runIndex = 0, .charInRun = firstWord.length() };
    EXPECT_EQ(extractSelectedText(doc, sel), firstWord);
}

TEST_F(MarkdownSelectionTests, ExtractAcrossRunsOnSameLine) {
    // "hello **bold** world" → ["hello", " ", "bold", " ", "world"] (or
    // similar). Selecting from start of "hello" through end of "bold".
    const auto doc = layout("hello **bold** world");
    const auto& line = doc.lines.at(0);
    const std::size_t boldRun = findRun(line, "bold");
    ASSERT_LT(boldRun, line.runs.size());

    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 0, .runIndex = boldRun, .charInRun = line.runs.at(boldRun).text.length() };
    const wxString text = extractSelectedText(doc, sel);
    EXPECT_TRUE(text.Contains("hello"));
    EXPECT_TRUE(text.Contains("bold"));
    EXPECT_FALSE(text.Contains("world"));
}

TEST_F(MarkdownSelectionTests, ExtractAcrossLinesJoinsWithNewline) {
    const auto doc = layout("first paragraph\n\nsecond paragraph");
    ASSERT_GE(doc.lines.size(), 2U);

    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 1, .runIndex = 0, .charInRun = doc.lines.at(1).runs.at(0).text.length() };
    const wxString text = extractSelectedText(doc, sel);
    EXPECT_TRUE(text.Contains("first"));
    EXPECT_TRUE(text.Contains("second"));
    EXPECT_TRUE(text.Contains("\n"));
}

// ---------------------------------------------------------------------------
// selectionToOffset / selectionFromOffset
// ---------------------------------------------------------------------------

TEST_F(MarkdownSelectionTests, OffsetRoundTripForStartOfDoc) {
    const auto doc = layout("hello world\n\nsecond paragraph");
    const SelectionPosition start { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    const std::size_t off = selectionToOffset(doc, start);
    EXPECT_EQ(off, 0U);
    const SelectionPosition recovered = selectionFromOffset(doc, off);
    EXPECT_EQ(recovered, start);
}

TEST_F(MarkdownSelectionTests, OffsetRoundTripMidRun) {
    const auto doc = layout("hello world");
    const wxString first = "hello";
    const SelectionPosition mid { .lineIndex = 0, .runIndex = 0, .charInRun = 3 };
    const std::size_t off = selectionToOffset(doc, mid);
    EXPECT_EQ(off, 3U);
    EXPECT_EQ(selectionFromOffset(doc, off), mid);
    EXPECT_LT(off, first.length());
}

TEST_F(MarkdownSelectionTests, OffsetSurvivesRewrapToNarrowerWidth) {
    // Long paragraph that wraps differently at 500px vs 80px.
    const wxString markdown = "one two three four five six seven eight nine ten";
    const FakeMeasurer measurer;
    const auto wide = layoutMarkdown(parseMarkdown(markdown), kTestWidth, measurer, fakePalette(), splitHighlight);
    // Pick a selection that spans multiple words in the wide layout.
    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    sel.caret = { .lineIndex = 0, .runIndex = findRun(wide.lines.at(0), "five"), .charInRun = wxString("five").length() };
    const wxString wideText = extractSelectedText(wide, sel);
    const std::size_t anchorOff = selectionToOffset(wide, sel.anchor);
    const std::size_t caretOff = selectionToOffset(wide, sel.caret);

    // Re-lay narrower so the line wrapping changes.
    constexpr int kNarrowWidth = 80;
    const auto narrow = layoutMarkdown(parseMarkdown(markdown), kNarrowWidth, measurer, fakePalette(), splitHighlight);
    EXPECT_GT(narrow.lines.size(), wide.lines.size());

    Selection remapped;
    remapped.anchor = selectionFromOffset(narrow, anchorOff);
    remapped.caret = selectionFromOffset(narrow, caretOff);
    // `extractSelectedText` joins lines with `\n`, so the literal text
    // legitimately differs after a re-wrap — strip line breaks to
    // compare the same set of characters was selected.
    const auto stripBreaks = [](wxString text) {
        text.Replace("\n", "");
        return text;
    };
    EXPECT_EQ(stripBreaks(extractSelectedText(narrow, remapped)), stripBreaks(wideText));
}

TEST_F(MarkdownSelectionTests, OffsetPastEndClampsToLastPosition) {
    const auto doc = layout("hello");
    const auto recovered = selectionFromOffset(doc, 10'000);
    // Past every character — clamp to the end of the last run.
    EXPECT_EQ(recovered.lineIndex, doc.lines.size() - 1);
    const auto& lastLine = doc.lines.at(recovered.lineIndex);
    EXPECT_EQ(recovered.runIndex, lastLine.runs.size() - 1);
    EXPECT_EQ(recovered.charInRun, lastLine.runs.back().text.length());
}

TEST_F(MarkdownSelectionTests, OffsetAtLineBoundaryPrefersNextLineStart) {
    // Two paragraphs — the second one starts on its own laid-out line.
    // A selection anchored at the start of that second line should
    // round-trip back to the same start-of-line position, not to the
    // (logically equivalent) end of the previous line — otherwise the
    // selection highlight grabs the inter-block gap on every resize.
    const auto doc = layout("first paragraph\n\nsecond paragraph");
    std::size_t secondLine = doc.lines.size();
    for (std::size_t li = 0; li < doc.lines.size(); li++) {
        for (const auto& run : doc.lines.at(li).runs) {
            if (run.text.StartsWith("second")) {
                secondLine = li;
                break;
            }
        }
        if (secondLine != doc.lines.size()) {
            break;
        }
    }
    ASSERT_LT(secondLine, doc.lines.size());
    const SelectionPosition lineStart { .lineIndex = secondLine, .runIndex = 0, .charInRun = 0 };
    const std::size_t off = selectionToOffset(doc, lineStart);
    EXPECT_EQ(selectionFromOffset(doc, off), lineStart);
}

TEST_F(MarkdownSelectionTests, OffsetAtLineEndPrefersSameLineForRangeEnd) {
    // Selection that ends at the last character of the first paragraph.
    // The end position must stay on that line — if it jumps forward to
    // `{nextLine, 0, 0}` the painter extends the highlight to the right
    // edge of the content and into the inter-block gap.
    const auto doc = layout("first paragraph\n\nsecond paragraph");
    std::size_t firstLineEnd = 0;
    for (std::size_t li = 0; li < doc.lines.size(); li++) {
        const auto& runs = doc.lines.at(li).runs;
        if (!runs.empty() && runs.front().text.StartsWith("first")) {
            firstLineEnd = li;
            // Find the last non-empty run on the line.
            for (std::size_t r = runs.size(); r > 0; r--) {
                if (!runs.at(r - 1).text.empty()) {
                    const SelectionPosition lineEnd { .lineIndex = li,
                        .runIndex = r - 1,
                        .charInRun = runs.at(r - 1).text.length() };
                    const std::size_t off = selectionToOffset(doc, lineEnd);
                    // The high end of the range — request the end-of-line bias.
                    EXPECT_EQ(selectionFromOffset(doc, off, OffsetBias::PreferLineEnd), lineEnd);
                    return;
                }
            }
        }
    }
    FAIL() << "Could not find first paragraph's line in laid-out doc";
}

TEST_F(MarkdownSelectionTests, ExtractIsNormalisedAcrossBackwardsDrag) {
    const wxString word = "hello";
    const auto doc = layout(word);
    Selection sel;
    sel.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = word.length() };
    sel.caret = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    EXPECT_EQ(extractSelectedText(doc, sel), word);
}
