//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/chat/Markdown.hpp"

using namespace fbide;

namespace {

/// Concatenate every Text/Link fragment of a block — handy for asserting on
/// content without caring how md4c split the runs.
auto plainText(const MdBlock& block) -> wxString {
    wxString out;
    for (const auto& run : block.inlines) {
        out += run.text;
    }
    return out;
}

/// Same, for a single table cell — flatten its inline runs to plain text.
auto cellText(const MdTableCell& cell) -> wxString {
    wxString out;
    for (const auto& run : cell.inlines) {
        out += run.text;
    }
    return out;
}

} // namespace

class MarkdownTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Paragraphs
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, SingleParagraph) {
    const auto doc = parseMarkdown("Hello world");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::Paragraph);
    EXPECT_EQ(plainText(doc.blocks[0]), "Hello world");
}

TEST_F(MarkdownTests, TwoParagraphs) {
    const auto doc = parseMarkdown("First\n\nSecond");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(plainText(doc.blocks[0]), "First");
    EXPECT_EQ(plainText(doc.blocks[1]), "Second");
}

TEST_F(MarkdownTests, Empty) {
    EXPECT_TRUE(parseMarkdown("").blocks.empty());
}

// ---------------------------------------------------------------------------
// Inline styling
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, BoldRun) {
    const auto doc = parseMarkdown("plain **bold** plain");
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_FALSE(runs[0].style.bold);
    EXPECT_TRUE(runs[1].style.bold);
    EXPECT_EQ(runs[1].text, "bold");
    EXPECT_FALSE(runs[2].style.bold);
}

TEST_F(MarkdownTests, ItalicRun) {
    const auto doc = parseMarkdown("an *emphasised* word");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.italic);
    EXPECT_EQ(runs[1].text, "emphasised");
}

TEST_F(MarkdownTests, NestedBoldItalic) {
    const auto doc = parseMarkdown("**bold *both* bold**");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[0].style.bold);
    EXPECT_FALSE(runs[0].style.italic);
    EXPECT_TRUE(runs[1].style.bold);
    EXPECT_TRUE(runs[1].style.italic);
    EXPECT_TRUE(runs[2].style.bold);
    EXPECT_FALSE(runs[2].style.italic);
}

TEST_F(MarkdownTests, InlineCodeSpan) {
    const auto doc = parseMarkdown("call `Print` now");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.code);
    EXPECT_EQ(runs[1].text, "Print");
}

TEST_F(MarkdownTests, Strikethrough) {
    const auto doc = parseMarkdown("this ~~gone~~ here");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.strikethrough);
}

// ---------------------------------------------------------------------------
// Headings
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, HeadingLevels) {
    const auto doc = parseMarkdown("# One\n\n### Three");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::Heading);
    EXPECT_EQ(doc.blocks[0].headingLevel, 1U);
    EXPECT_EQ(plainText(doc.blocks[0]), "One");
    EXPECT_EQ(doc.blocks[1].headingLevel, 3U);
}

// ---------------------------------------------------------------------------
// Code fences
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, FencedCodeWithLang) {
    const auto doc = parseMarkdown("```freebasic\nPrint 1\nPrint 2\n```");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[0].codeLang, "freebasic");
    EXPECT_EQ(doc.blocks[0].codeText, "Print 1\nPrint 2\n");
}

TEST_F(MarkdownTests, FenceLangIsLowercased) {
    const auto doc = parseMarkdown("```FreeBASIC\nDim x\n```");
    EXPECT_EQ(doc.blocks[0].codeLang, "freebasic");
}

TEST_F(MarkdownTests, UnterminatedFenceStillYieldsBlock) {
    // Mid-stream: the closing fence has not arrived yet.
    const auto doc = parseMarkdown("```\nPrint 1\nPrint 2");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[0].codeText, "Print 1\nPrint 2\n");
}

TEST_F(MarkdownTests, CodeFenceKeepsMetacharactersVerbatim) {
    const auto doc = parseMarkdown("```\nIf a < b And c > d Then\n```");
    EXPECT_EQ(doc.blocks[0].codeText, "If a < b And c > d Then\n");
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, UnorderedList) {
    const auto doc = parseMarkdown("- one\n- two\n- three");
    ASSERT_EQ(doc.blocks.size(), 3U);
    for (const auto& block : doc.blocks) {
        EXPECT_EQ(block.kind, MdBlockKind::ListItem);
        EXPECT_FALSE(block.listOrdered);
        EXPECT_EQ(block.listDepth, 1);
        EXPECT_TRUE(block.listMarker);
    }
    EXPECT_EQ(plainText(doc.blocks[0]), "one");
    EXPECT_EQ(plainText(doc.blocks[2]), "three");
}

TEST_F(MarkdownTests, OrderedListOrdinals) {
    const auto doc = parseMarkdown("1. first\n2. second\n3. third");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_TRUE(doc.blocks[0].listOrdered);
    EXPECT_EQ(doc.blocks[0].listOrdinal, 1);
    EXPECT_EQ(doc.blocks[1].listOrdinal, 2);
    EXPECT_EQ(doc.blocks[2].listOrdinal, 3);
}

TEST_F(MarkdownTests, OrderedListHonoursStart) {
    const auto doc = parseMarkdown("5. five\n6. six");
    EXPECT_EQ(doc.blocks[0].listOrdinal, 5);
    EXPECT_EQ(doc.blocks[1].listOrdinal, 6);
}

TEST_F(MarkdownTests, NestedListDepth) {
    const auto doc = parseMarkdown("- outer\n    - inner");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0].listDepth, 1);
    EXPECT_EQ(doc.blocks[1].listDepth, 2);
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, InlineLink) {
    const auto doc = parseMarkdown("see [the docs](https://example.org) now");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::Link);
    EXPECT_EQ(runs[1].text, "the docs");
    EXPECT_EQ(runs[1].url, "https://example.org");
    EXPECT_EQ(runs[0].kind, MdInlineKind::Text);
}

// ---------------------------------------------------------------------------
// Breaks
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, SoftBreakInsideParagraph) {
    const auto doc = parseMarkdown("line one\nline two");
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::SoftBreak);
}

TEST_F(MarkdownTests, HardBreakInsideParagraph) {
    const auto doc = parseMarkdown("line one  \nline two");
    const auto& runs = doc.blocks[0].inlines;
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::HardBreak);
}

// ---------------------------------------------------------------------------
// Block quotes / rules
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, BlockQuoteDepth) {
    const auto doc = parseMarkdown("> quoted text");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0].quoteDepth, 1);
    EXPECT_EQ(plainText(doc.blocks[0]), "quoted text");
}

TEST_F(MarkdownTests, HorizontalRule) {
    const auto doc = parseMarkdown("above\n\n---\n\nbelow");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_EQ(doc.blocks[1].kind, MdBlockKind::Rule);
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, NamedEntitiesDecoded) {
    const auto doc = parseMarkdown("a &amp; b &lt; c");
    EXPECT_EQ(plainText(doc.blocks[0]), "a & b < c");
}

// ---------------------------------------------------------------------------
// Mixed document
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, ProseThenCodeThenProse) {
    const auto doc = parseMarkdown("Here is code:\n\n```fb\nPrint 1\n```\n\nDone.");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::Paragraph);
    EXPECT_EQ(doc.blocks[1].kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[2].kind, MdBlockKind::Paragraph);
}

// ---------------------------------------------------------------------------
// GFM tables
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, SimpleTableParsedAsTableBlock) {
    const auto doc = parseMarkdown(
        "| Name | Type |\n"
        "|------|------|\n"
        "| Foo  | Int  |\n"
        "| Bar  | Str  |\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& table = doc.blocks[0];
    EXPECT_EQ(table.kind, MdBlockKind::Table);
    EXPECT_EQ(table.headerRowCount, 1U);
    ASSERT_EQ(table.rows.size(), 3U);
    ASSERT_EQ(table.rows[0].cells.size(), 2U);
    EXPECT_EQ(cellText(table.rows[0].cells[0]), "Name");
    EXPECT_EQ(cellText(table.rows[0].cells[1]), "Type");
    EXPECT_EQ(cellText(table.rows[1].cells[0]), "Foo");
    EXPECT_EQ(cellText(table.rows[1].cells[1]), "Int");
    EXPECT_EQ(cellText(table.rows[2].cells[0]), "Bar");
    EXPECT_EQ(cellText(table.rows[2].cells[1]), "Str");
}

TEST_F(MarkdownTests, TableSeparatorMarkersDriveColumnAlignment) {
    const auto doc = parseMarkdown(
        "| L | C | R |\n"
        "|:--|:-:|--:|\n"
        "| 1 | 2 | 3 |\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& table = doc.blocks[0];
    ASSERT_EQ(table.columnAlignment.size(), 3U);
    EXPECT_EQ(table.columnAlignment[0], MdTableAlignment::Left);
    EXPECT_EQ(table.columnAlignment[1], MdTableAlignment::Center);
    EXPECT_EQ(table.columnAlignment[2], MdTableAlignment::Right);
}

TEST_F(MarkdownTests, InlineFormattingInTableCells) {
    const auto doc = parseMarkdown(
        "| Plain | Styled            |\n"
        "|-------|-------------------|\n"
        "| x     | **bold** `code`   |\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& table = doc.blocks[0];
    ASSERT_EQ(table.rows.size(), 2U);
    const auto& styledCell = table.rows[1].cells[1];
    // Three inline runs: "bold" with bold style, " ", "code" with code style.
    bool sawBold = false;
    bool sawCode = false;
    for (const auto& run : styledCell.inlines) {
        if (run.style.bold && run.text == "bold") {
            sawBold = true;
        }
        if (run.style.code && run.text == "code") {
            sawCode = true;
        }
    }
    EXPECT_TRUE(sawBold);
    EXPECT_TRUE(sawCode);
}

TEST_F(MarkdownTests, HeaderRowAloneIsNotATable) {
    // GFM requires the separator row to commit to a table. A naked
    // header row should be parsed as a paragraph, leaving the streaming
    // case (header arrives before separator) un-broken.
    const auto doc = parseMarkdown("| not | a table |\n");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0].kind, MdBlockKind::Paragraph);
}
