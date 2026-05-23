//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "markdown/Markdown.hpp"
#include "markdown/MarkdownLayout.hpp"

using namespace fbide;

namespace {

// Deterministic measurer: every glyph is a fixed width, line height tracks
// the size delta. Proportional text is 10px/char, monospace 8px/char.
class FakeMeasurer final : public TextMeasurer {
public:
    auto width(const wxString& text, const TextStyle& style) const -> int override {
        return static_cast<int>(text.length()) * (style.monospace ? 8 : 10);
    }
    auto lineHeight(const TextStyle& style) const -> int override {
        return 20 + style.sizeDelta;
    }
};

/// Trivial fence highlighter — one black run per code line, '\n'-split, with
/// the trailing blank line dropped (as the real highlighter does).
auto splitHighlight(const wxString& code, const wxString& /*lang*/) -> std::vector<CodeLine> {
    std::vector<CodeLine> lines;
    CodeLine current;
    wxString segment;
    for (const wxUniChar ch : code) {
        if (ch == '\n') {
            if (!segment.empty()) {
                current.push_back({ .text = segment, .colour = wxColour(0, 0, 0) });
            }
            lines.push_back(current);
            current.clear();
            segment.clear();
        } else {
            segment += ch;
        }
    }
    if (!segment.empty()) {
        current.push_back({ .text = segment, .colour = wxColour(0, 0, 0) });
    }
    lines.push_back(current);
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

auto palette() -> MarkdownPalette {
    return { .text = wxColour(0, 0, 0),
        .link = wxColour(0, 0, 200),
        .codeBg = wxColour(240, 240, 240),
        .inlineCodeBg = wxColour(230, 230, 230),
        .rule = wxColour(200, 200, 200) };
}

/// Lay out `markdown` at `width` pixels through the fakes.
auto layout(const wxString& markdown, const int width) -> LaidOutDoc {
    const FakeMeasurer measurer;
    return layoutMarkdown(parseMarkdown(markdown), width, measurer, palette(), splitHighlight);
}

/// Lay out with a custom image resolver — used by the image tests so the
/// layout can be exercised without the real download path.
auto layoutWithImages(const wxString& markdown, const int width, const ImageResolver& resolver) -> LaidOutDoc {
    const FakeMeasurer measurer;
    return layoutMarkdown(parseMarkdown(markdown), width, measurer, palette(), splitHighlight, resolver);
}

} // namespace

class MarkdownLayoutTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Basics
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, EmptyDocument) {
    const auto doc = layout("", 500);
    EXPECT_TRUE(doc.lines.empty());
    EXPECT_EQ(doc.height, 0);
    EXPECT_EQ(doc.width, 500);
}

TEST_F(MarkdownLayoutTests, SingleShortParagraphIsOneLine) {
    const auto doc = layout("hello world", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Prose);
    EXPECT_EQ(doc.lines[0].y, 0);
    EXPECT_EQ(doc.lines[0].height, 20);
    EXPECT_EQ(doc.height, 20);
}

// ---------------------------------------------------------------------------
// Word wrap
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, ParagraphWrapsAtWidth) {
    // Words are 40px (4 chars), spaces 10px. Width 100 fits "aaaa bbbb"
    // (40+10+40 = 90) but not a third word.
    const auto doc = layout("aaaa bbbb cccc", 100);
    ASSERT_EQ(doc.lines.size(), 2U);
    ASSERT_EQ(doc.lines[0].runs.size(), 2U);
    EXPECT_EQ(doc.lines[0].runs[0].text, "aaaa");
    EXPECT_EQ(doc.lines[0].runs[0].x, 0);
    EXPECT_EQ(doc.lines[0].runs[1].text, "bbbb");
    EXPECT_EQ(doc.lines[0].runs[1].x, 50);
    ASSERT_EQ(doc.lines[1].runs.size(), 1U);
    EXPECT_EQ(doc.lines[1].runs[0].text, "cccc");
    EXPECT_EQ(doc.lines[1].runs[0].x, 0);
    EXPECT_EQ(doc.lines[1].y, 20);
    EXPECT_EQ(doc.height, 40);
}

TEST_F(MarkdownLayoutTests, WideEnoughDocumentDoesNotWrap) {
    const auto doc = layout("aaaa bbbb cccc", 500);
    EXPECT_EQ(doc.lines.size(), 1U);
}

TEST_F(MarkdownLayoutTests, HardBreakForcesNewLine) {
    // Two spaces + newline is a markdown hard break.
    const auto doc = layout("one  \ntwo", 500);
    ASSERT_EQ(doc.lines.size(), 2U);
    EXPECT_EQ(doc.lines[0].runs[0].text, "one");
    EXPECT_EQ(doc.lines[1].runs[0].text, "two");
}

// ---------------------------------------------------------------------------
// Block stacking
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, ParagraphsAreSeparatedByABlockGap) {
    const auto doc = layout("one\n\ntwo", 500);
    ASSERT_EQ(doc.lines.size(), 2U);
    EXPECT_EQ(doc.lines[0].y, 0);
    EXPECT_EQ(doc.lines[1].y, 28); // 20 line + 8 gap
    EXPECT_EQ(doc.height, 48);
}

TEST_F(MarkdownLayoutTests, HeadingUsesALargerLineHeight) {
    const auto doc = layout("# Title", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].height, 30); // body 20 + level-1 delta 10
}

// ---------------------------------------------------------------------------
// Code fences
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, CodeBlockHasPaddingStripsAndCodeLines) {
    const auto doc = layout("```\nab\ncd\n```", 500);
    // top pad + 2 code lines + bottom pad
    ASSERT_EQ(doc.lines.size(), 4U);
    for (const auto& line : doc.lines) {
        EXPECT_EQ(line.kind, LineKind::Code);
    }
    EXPECT_EQ(doc.lines[0].height, 8);  // top padding
    EXPECT_EQ(doc.lines[1].height, 20); // code line
    EXPECT_EQ(doc.lines[2].height, 20);
    EXPECT_EQ(doc.lines[3].height, 8); // bottom padding
    EXPECT_EQ(doc.height, 56);
}

TEST_F(MarkdownLayoutTests, CodeBlockRegionIsRecorded) {
    const auto doc = layout("text\n\n```fb\nab\ncd\n```", 500);
    ASSERT_EQ(doc.codeBlocks.size(), 1U);
    // Snippet body and language live on the source markdown — resolved on
    // demand via `resolveCodeBlockText`. The laid block only carries the
    // y/height the painter and hit-tester need.
    // The region spans every Code line of the block.
    int codeTop = -1;
    int codeBottom = 0;
    for (const auto& line : doc.lines) {
        if (line.kind == LineKind::Code) {
            if (codeTop < 0) {
                codeTop = line.y;
            }
            codeBottom = line.y + line.height;
        }
    }
    EXPECT_EQ(doc.codeBlocks[0].y, codeTop);
    EXPECT_EQ(doc.codeBlocks[0].height, codeBottom - codeTop);
}

TEST_F(MarkdownLayoutTests, CodeRunsAreMonospaceAndIndentedByPadding) {
    const auto doc = layout("```\nab\n```", 500);
    ASSERT_EQ(doc.lines.size(), 3U);
    ASSERT_EQ(doc.lines[1].runs.size(), 1U);
    const auto& run = doc.lines[1].runs[0];
    EXPECT_EQ(run.text, "ab");
    EXPECT_TRUE(run.style.monospace);
    EXPECT_EQ(run.x, 8);      // left + code padding
    EXPECT_EQ(run.width, 16); // 2 chars * 8px monospace
}

TEST_F(MarkdownLayoutTests, LongCodeLineSoftWraps) {
    // Monospace is 8px/char; at width 100 a code line wraps. The first line
    // starts at the code padding (8), continuation lines are further indented.
    const auto doc = layout("```\nabcdefghijklmnopqrstuvwxy\n```", 100);
    // top pad + 3 wrapped lines + bottom pad
    ASSERT_EQ(doc.lines.size(), 5U);
    for (const auto& line : doc.lines) {
        EXPECT_EQ(line.kind, LineKind::Code);
    }
    EXPECT_EQ(doc.lines[1].runs[0].x, 8);  // first line at code padding
    EXPECT_EQ(doc.lines[2].runs[0].x, 24); // continuation indent
    EXPECT_EQ(doc.lines[3].runs[0].x, 24);
}

TEST_F(MarkdownLayoutTests, ShortCodeLineDoesNotWrap) {
    const auto doc = layout("```\nshort\n```", 500);
    ASSERT_EQ(doc.lines.size(), 3U); // top pad + 1 line + bottom pad
    ASSERT_EQ(doc.lines[1].runs.size(), 1U);
    EXPECT_EQ(doc.lines[1].runs[0].text, "short");
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, HorizontalRuleIsARuleLine) {
    const auto doc = layout("above\n\n---\n\nbelow", 500);
    ASSERT_EQ(doc.lines.size(), 3U);
    EXPECT_EQ(doc.lines[1].kind, LineKind::Rule);
    EXPECT_EQ(doc.lines[1].height, 13);
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, ListItemHasAMarkerRunFirst) {
    const auto doc = layout("- item", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    ASSERT_GE(doc.lines[0].runs.size(), 2U);
    const auto& marker = doc.lines[0].runs[0];
    EXPECT_EQ(marker.text, wxString(wxUniChar(0x2022)) + " ");
    EXPECT_EQ(doc.lines[0].runs[1].text, "item");
    EXPECT_EQ(doc.lines[0].runs[1].x, 24); // one list-indent level
}

TEST_F(MarkdownLayoutTests, OrderedListMarkerShowsTheOrdinal) {
    const auto doc = layout("3. third", 500);
    ASSERT_GE(doc.lines[0].runs.size(), 1U);
    EXPECT_EQ(doc.lines[0].runs[0].text, "3. ");
}

TEST_F(MarkdownLayoutTests, TaskListMarkerReflectsCheckedState) {
    const auto doc = layout("- [ ] todo\n- [x] done", 500);
    ASSERT_EQ(doc.lines.size(), 2U);
    ASSERT_GE(doc.lines[0].runs.size(), 1U);
    ASSERT_GE(doc.lines[1].runs.size(), 1U);
    // U+2610 BALLOT BOX, U+2611 BALLOT BOX WITH CHECK.
    EXPECT_EQ(doc.lines[0].runs[0].text, wxString(wxUniChar(0x2610)) + " ");
    EXPECT_EQ(doc.lines[1].runs[0].text, wxString(wxUniChar(0x2611)) + " ");
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, ReadyImageBecomesItsOwnImageLine) {
    // 100x40 bitmap fits inside a 500px width, so no scaling.
    const wxImage rawImage(100, 40);
    const wxBitmap bitmap(rawImage);
    const auto resolver = [&bitmap](const wxString& /*url*/) -> ImageInfo {
        return { .state = ImageInfo::State::Ready,
            .bitmap = bitmap,
            .width = 100,
            .height = 40 };
    };
    const auto doc = layoutWithImages("![cat](https://e.org/c.png)", 500, resolver);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Image);
    EXPECT_EQ(doc.lines[0].image.drawWidth, 100);
    EXPECT_EQ(doc.lines[0].image.drawHeight, 40);
    EXPECT_EQ(doc.lines[0].image.url, "https://e.org/c.png");
    EXPECT_EQ(doc.lines[0].image.alt, "cat");
    ASSERT_EQ(doc.links.size(), 1U);
    EXPECT_EQ(doc.links[0].url, "https://e.org/c.png");
    // The image line has one hit-test run carrying the link id, no text.
    ASSERT_EQ(doc.lines[0].runs.size(), 1U);
    EXPECT_EQ(doc.lines[0].runs[0].linkId, 0);
}

TEST_F(MarkdownLayoutTests, WideImageScalesProportionallyToFitWidth) {
    // 800x200 → 400-wide bubble → drawn at 400x100.
    const wxImage rawImage(800, 200);
    const wxBitmap bitmap(rawImage);
    const auto resolver = [&bitmap](const wxString&) -> ImageInfo {
        return { .state = ImageInfo::State::Ready,
            .bitmap = bitmap,
            .width = 800,
            .height = 200 };
    };
    const auto doc = layoutWithImages("![big](https://e.org/b.png)", 400, resolver);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Image);
    EXPECT_EQ(doc.lines[0].image.drawWidth, 400);
    EXPECT_EQ(doc.lines[0].image.drawHeight, 100);
}

TEST_F(MarkdownLayoutTests, LoadingImageRendersAsPlaceholderProseLine) {
    const auto resolver = [](const wxString&) -> ImageInfo {
        return { .state = ImageInfo::State::Loading };
    };
    const auto doc = layoutWithImages("![cat](https://e.org/c.png)", 500, resolver);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Prose);
    ASSERT_EQ(doc.lines[0].runs.size(), 1U);
    EXPECT_TRUE(doc.lines[0].runs[0].text.Contains("cat"));
    EXPECT_TRUE(doc.lines[0].runs[0].text.Contains("loading"));
    EXPECT_TRUE(doc.lines[0].runs[0].style.underline);
    EXPECT_EQ(doc.lines[0].runs[0].linkId, 0);
}

TEST_F(MarkdownLayoutTests, FailedImageRendersAsPlaceholderProseLine) {
    const auto resolver = [](const wxString&) -> ImageInfo {
        return { .state = ImageInfo::State::Failed };
    };
    const auto doc = layoutWithImages("![cat](https://e.org/c.png)", 500, resolver);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Prose);
    EXPECT_TRUE(doc.lines[0].runs[0].text.Contains("failed"));
}

TEST_F(MarkdownLayoutTests, ImageBetweenTextSplitsParagraphIntoThreeLines) {
    const wxImage rawImage(50, 50);
    const wxBitmap bitmap(rawImage);
    const auto resolver = [&bitmap](const wxString&) -> ImageInfo {
        return { .state = ImageInfo::State::Ready,
            .bitmap = bitmap,
            .width = 50,
            .height = 50 };
    };
    const auto doc = layoutWithImages("before ![x](https://e.org/x.png) after", 500, resolver);
    // The image becomes a block-level break: "before" line, image line, "after" line.
    ASSERT_EQ(doc.lines.size(), 3U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Prose);
    EXPECT_EQ(doc.lines[1].kind, LineKind::Image);
    EXPECT_EQ(doc.lines[2].kind, LineKind::Prose);
}

TEST_F(MarkdownLayoutTests, ImageWithoutResolverFallsBackToFailedPlaceholder) {
    // No resolver provided — every image is treated as permanently Failed.
    const auto doc = layout("![cat](https://e.org/c.png)", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::Prose);
    EXPECT_TRUE(doc.lines[0].runs[0].text.Contains("failed"));
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, LinkRunIsRegisteredAndTagged) {
    const auto doc = layout("see [docs](https://example.org)", 500);
    ASSERT_EQ(doc.links.size(), 1U);
    EXPECT_EQ(doc.links[0].url, "https://example.org");

    // Find the run carrying the link.
    int tagged = -1;
    for (const auto& line : doc.lines) {
        for (const auto& run : line.runs) {
            if (run.linkId >= 0) {
                tagged = run.linkId;
                EXPECT_EQ(run.text, "docs");
                EXPECT_TRUE(run.style.underline);
            }
        }
    }
    EXPECT_EQ(tagged, 0);
}

// ---------------------------------------------------------------------------
// Quote indent
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, BlockQuoteIndentsAndRecordsDepth) {
    const auto doc = layout("> quoted", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].quoteDepth, 1);
    EXPECT_EQ(doc.lines[0].runs[0].x, 16); // one quote-indent level
}

// ---------------------------------------------------------------------------
// Patch proposals
// ---------------------------------------------------------------------------

TEST_F(MarkdownLayoutTests, PatchBlockEmitsSearchAndReplaceStrips) {
    const auto doc = layout(
        "<<<<<<< SEARCH\n"
        "old\n"
        "=======\n"
        "new\n"
        ">>>>>>> REPLACE\n",
        500
    );
    // Two padding strips per half + one content line per half = 6 lines.
    ASSERT_EQ(doc.lines.size(), 6U);
    EXPECT_EQ(doc.lines[0].kind, LineKind::PatchSearch);  // top pad
    EXPECT_EQ(doc.lines[1].kind, LineKind::PatchSearch);  // "old"
    EXPECT_EQ(doc.lines[2].kind, LineKind::PatchSearch);  // bottom pad
    EXPECT_EQ(doc.lines[3].kind, LineKind::PatchReplace); // top pad
    EXPECT_EQ(doc.lines[4].kind, LineKind::PatchReplace); // "new"
    EXPECT_EQ(doc.lines[5].kind, LineKind::PatchReplace); // bottom pad
}

TEST_F(MarkdownLayoutTests, PatchBlockRegionIsRecorded) {
    const auto doc = layout(
        "<<<<<<< SEARCH\n"
        "a\n"
        "=======\n"
        "b\n"
        ">>>>>>> REPLACE\n",
        500
    );
    ASSERT_EQ(doc.patchBlocks.size(), 1U);
    EXPECT_EQ(doc.patchBlocks[0].search, "a\n");
    EXPECT_EQ(doc.patchBlocks[0].replace, "b\n");
    EXPECT_EQ(doc.patchBlocks[0].y, doc.lines.front().y);
    EXPECT_EQ(doc.patchBlocks[0].height,
        doc.lines.back().y + doc.lines.back().height - doc.lines.front().y);
}

TEST_F(MarkdownLayoutTests, PatchBlockCarriesTargetThrough) {
    const auto doc = layout(
        "<<<<<<< SEARCH foo.bas\n"
        "x\n"
        "=======\n"
        "y\n"
        ">>>>>>> REPLACE\n",
        500
    );
    ASSERT_EQ(doc.patchBlocks.size(), 1U);
    EXPECT_EQ(doc.patchBlocks[0].target, "foo.bas");
}
