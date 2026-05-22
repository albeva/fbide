//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/chat/ChatLayout.hpp"
#include "ai/chat/Markdown.hpp"

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

auto palette() -> ChatPalette {
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

} // namespace

class ChatLayoutTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Basics
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, EmptyDocument) {
    const auto doc = layout("", 500);
    EXPECT_TRUE(doc.lines.empty());
    EXPECT_EQ(doc.height, 0);
    EXPECT_EQ(doc.width, 500);
}

TEST_F(ChatLayoutTests, SingleShortParagraphIsOneLine) {
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

TEST_F(ChatLayoutTests, ParagraphWrapsAtWidth) {
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

TEST_F(ChatLayoutTests, WideEnoughDocumentDoesNotWrap) {
    const auto doc = layout("aaaa bbbb cccc", 500);
    EXPECT_EQ(doc.lines.size(), 1U);
}

TEST_F(ChatLayoutTests, HardBreakForcesNewLine) {
    // Two spaces + newline is a markdown hard break.
    const auto doc = layout("one  \ntwo", 500);
    ASSERT_EQ(doc.lines.size(), 2U);
    EXPECT_EQ(doc.lines[0].runs[0].text, "one");
    EXPECT_EQ(doc.lines[1].runs[0].text, "two");
}

// ---------------------------------------------------------------------------
// Block stacking
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, ParagraphsAreSeparatedByABlockGap) {
    const auto doc = layout("one\n\ntwo", 500);
    ASSERT_EQ(doc.lines.size(), 2U);
    EXPECT_EQ(doc.lines[0].y, 0);
    EXPECT_EQ(doc.lines[1].y, 28); // 20 line + 8 gap
    EXPECT_EQ(doc.height, 48);
}

TEST_F(ChatLayoutTests, HeadingUsesALargerLineHeight) {
    const auto doc = layout("# Title", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].height, 30); // body 20 + level-1 delta 10
}

// ---------------------------------------------------------------------------
// Code fences
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, CodeBlockHasPaddingStripsAndCodeLines) {
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

TEST_F(ChatLayoutTests, CodeBlockRegionIsRecorded) {
    const auto doc = layout("text\n\n```fb\nab\ncd\n```", 500);
    ASSERT_EQ(doc.codeBlocks.size(), 1U);
    EXPECT_EQ(doc.codeBlocks[0].code, "ab\ncd\n");
    EXPECT_EQ(doc.codeBlocks[0].lang, "fb");
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

TEST_F(ChatLayoutTests, CodeRunsAreMonospaceAndIndentedByPadding) {
    const auto doc = layout("```\nab\n```", 500);
    ASSERT_EQ(doc.lines.size(), 3U);
    ASSERT_EQ(doc.lines[1].runs.size(), 1U);
    const auto& run = doc.lines[1].runs[0];
    EXPECT_EQ(run.text, "ab");
    EXPECT_TRUE(run.style.monospace);
    EXPECT_EQ(run.x, 8);      // left + code padding
    EXPECT_EQ(run.width, 16); // 2 chars * 8px monospace
}

TEST_F(ChatLayoutTests, LongCodeLineSoftWraps) {
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

TEST_F(ChatLayoutTests, ShortCodeLineDoesNotWrap) {
    const auto doc = layout("```\nshort\n```", 500);
    ASSERT_EQ(doc.lines.size(), 3U); // top pad + 1 line + bottom pad
    ASSERT_EQ(doc.lines[1].runs.size(), 1U);
    EXPECT_EQ(doc.lines[1].runs[0].text, "short");
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, HorizontalRuleIsARuleLine) {
    const auto doc = layout("above\n\n---\n\nbelow", 500);
    ASSERT_EQ(doc.lines.size(), 3U);
    EXPECT_EQ(doc.lines[1].kind, LineKind::Rule);
    EXPECT_EQ(doc.lines[1].height, 13);
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, ListItemHasAMarkerRunFirst) {
    const auto doc = layout("- item", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    ASSERT_GE(doc.lines[0].runs.size(), 2U);
    const auto& marker = doc.lines[0].runs[0];
    EXPECT_EQ(marker.text, wxString(wxUniChar(0x2022)) + " ");
    EXPECT_EQ(doc.lines[0].runs[1].text, "item");
    EXPECT_EQ(doc.lines[0].runs[1].x, 24); // one list-indent level
}

TEST_F(ChatLayoutTests, OrderedListMarkerShowsTheOrdinal) {
    const auto doc = layout("3. third", 500);
    ASSERT_GE(doc.lines[0].runs.size(), 1U);
    EXPECT_EQ(doc.lines[0].runs[0].text, "3. ");
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, LinkRunIsRegisteredAndTagged) {
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

TEST_F(ChatLayoutTests, BlockQuoteIndentsAndRecordsDepth) {
    const auto doc = layout("> quoted", 500);
    ASSERT_EQ(doc.lines.size(), 1U);
    EXPECT_EQ(doc.lines[0].quoteDepth, 1);
    EXPECT_EQ(doc.lines[0].runs[0].x, 16); // one quote-indent level
}

// ---------------------------------------------------------------------------
// Patch proposals
// ---------------------------------------------------------------------------

TEST_F(ChatLayoutTests, PatchBlockEmitsSearchAndReplaceStrips) {
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

TEST_F(ChatLayoutTests, PatchBlockRegionIsRecorded) {
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

TEST_F(ChatLayoutTests, PatchBlockCarriesTargetThrough) {
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
