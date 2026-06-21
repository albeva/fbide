//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "markdown/Markdown.hpp"

using namespace fbide;
using namespace fbide::markdown;

namespace {

/// Concatenate every Text/Link fragment of a block — handy for asserting on
/// content without caring how md4c split the runs.
auto plainText(const MdBlockBase& block) -> wxString {
    wxString out;
    for (const auto& run : blockInlines(block)) {
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
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Paragraph);
    EXPECT_EQ(plainText(*doc.blocks[0]), "Hello world");
}

TEST_F(MarkdownTests, TwoParagraphs) {
    const auto doc = parseMarkdown("First\n\nSecond");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(plainText(*doc.blocks[0]), "First");
    EXPECT_EQ(plainText(*doc.blocks[1]), "Second");
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
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_FALSE(runs[0].style.bold);
    EXPECT_TRUE(runs[1].style.bold);
    EXPECT_EQ(runs[1].text, "bold");
    EXPECT_FALSE(runs[2].style.bold);
}

TEST_F(MarkdownTests, ItalicRun) {
    const auto doc = parseMarkdown("an *emphasised* word");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.italic);
    EXPECT_EQ(runs[1].text, "emphasised");
}

TEST_F(MarkdownTests, NestedBoldItalic) {
    const auto doc = parseMarkdown("**bold *both* bold**");
    const auto& runs = blockInlines(*doc.blocks[0]);
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
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.code);
    EXPECT_EQ(runs[1].text, "Print");
}

TEST_F(MarkdownTests, Strikethrough) {
    const auto doc = parseMarkdown("this ~~gone~~ here");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_TRUE(runs[1].style.strikethrough);
}

// ---------------------------------------------------------------------------
// Headings
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, HeadingLevels) {
    const auto doc = parseMarkdown("# One\n\n### Three");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Heading);
    EXPECT_EQ(doc.blocks[0]->as<MdHeading>().headingLevel, 1U);
    EXPECT_EQ(plainText(*doc.blocks[0]), "One");
    EXPECT_EQ(doc.blocks[1]->as<MdHeading>().headingLevel, 3U);
}

TEST_F(MarkdownTests, SetextHeadings) {
    // `=====` underline → H1, `-----` underline → H2.
    const auto doc = parseMarkdown("Title One\n=====\n\nTitle Two\n-----");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Heading);
    EXPECT_EQ(doc.blocks[0]->as<MdHeading>().headingLevel, 1U);
    EXPECT_EQ(plainText(*doc.blocks[0]), "Title One");
    EXPECT_EQ(doc.blocks[1]->kind, MdBlockKind::Heading);
    EXPECT_EQ(doc.blocks[1]->as<MdHeading>().headingLevel, 2U);
    EXPECT_EQ(plainText(*doc.blocks[1]), "Title Two");
}

// ---------------------------------------------------------------------------
// Code fences
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, FencedCodeWithLang) {
    const auto doc = parseMarkdown("```freebasic\nPrint 1\nPrint 2\n```");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[0]->as<MdCodeFence>().codeLang, "freebasic");
    EXPECT_EQ(doc.blocks[0]->as<MdCodeFence>().codeText, "Print 1\nPrint 2\n");
}

TEST_F(MarkdownTests, FenceLangIsLowercased) {
    const auto doc = parseMarkdown("```FreeBASIC\nDim x\n```");
    EXPECT_EQ(doc.blocks[0]->as<MdCodeFence>().codeLang, "freebasic");
}

TEST_F(MarkdownTests, UnterminatedFenceStillYieldsBlock) {
    // Mid-stream: the closing fence has not arrived yet.
    const auto doc = parseMarkdown("```\nPrint 1\nPrint 2");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[0]->as<MdCodeFence>().codeText, "Print 1\nPrint 2\n");
}

TEST_F(MarkdownTests, CodeFenceKeepsMetacharactersVerbatim) {
    const auto doc = parseMarkdown("```\nIf a < b And c > d Then\n```");
    EXPECT_EQ(doc.blocks[0]->as<MdCodeFence>().codeText, "If a < b And c > d Then\n");
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, UnorderedList) {
    const auto doc = parseMarkdown("- one\n- two\n- three");
    ASSERT_EQ(doc.blocks.size(), 3U);
    for (const auto& block : doc.blocks) {
        EXPECT_EQ(block->kind, MdBlockKind::ListItem);
        EXPECT_FALSE(block->as<MdListItem>().listOrdered);
        EXPECT_EQ(block->as<MdListItem>().listDepth, 1);
        EXPECT_TRUE(block->as<MdListItem>().listMarker);
    }
    EXPECT_EQ(plainText(*doc.blocks[0]), "one");
    EXPECT_EQ(plainText(*doc.blocks[2]), "three");
}

TEST_F(MarkdownTests, OrderedListOrdinals) {
    const auto doc = parseMarkdown("1. first\n2. second\n3. third");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_TRUE(doc.blocks[0]->as<MdListItem>().listOrdered);
    EXPECT_EQ(doc.blocks[0]->as<MdListItem>().listOrdinal, 1);
    EXPECT_EQ(doc.blocks[1]->as<MdListItem>().listOrdinal, 2);
    EXPECT_EQ(doc.blocks[2]->as<MdListItem>().listOrdinal, 3);
}

TEST_F(MarkdownTests, OrderedListHonoursStart) {
    const auto doc = parseMarkdown("5. five\n6. six");
    EXPECT_EQ(doc.blocks[0]->as<MdListItem>().listOrdinal, 5);
    EXPECT_EQ(doc.blocks[1]->as<MdListItem>().listOrdinal, 6);
}

TEST_F(MarkdownTests, NestedListDepth) {
    const auto doc = parseMarkdown("- outer\n    - inner");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0]->as<MdListItem>().listDepth, 1);
    EXPECT_EQ(doc.blocks[1]->as<MdListItem>().listDepth, 2);
}

TEST_F(MarkdownTests, TaskListUncheckedAndChecked) {
    const auto doc = parseMarkdown("- [ ] todo\n- [x] done");
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_TRUE(doc.blocks[0]->as<MdListItem>().isTask);
    EXPECT_FALSE(doc.blocks[0]->as<MdListItem>().taskChecked);
    EXPECT_EQ(plainText(*doc.blocks[0]), "todo");
    EXPECT_TRUE(doc.blocks[1]->as<MdListItem>().isTask);
    EXPECT_TRUE(doc.blocks[1]->as<MdListItem>().taskChecked);
    EXPECT_EQ(plainText(*doc.blocks[1]), "done");
}

TEST_F(MarkdownTests, TaskListUppercaseTickAlsoChecked) {
    const auto doc = parseMarkdown("- [X] also done");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_TRUE(doc.blocks[0]->as<MdListItem>().isTask);
    EXPECT_TRUE(doc.blocks[0]->as<MdListItem>().taskChecked);
}

TEST_F(MarkdownTests, PlainListItemIsNotATask) {
    const auto doc = parseMarkdown("- regular");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_FALSE(doc.blocks[0]->as<MdListItem>().isTask);
    EXPECT_FALSE(doc.blocks[0]->as<MdListItem>().taskChecked);
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, InlineLink) {
    const auto doc = parseMarkdown("see [the docs](https://example.org) now");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::Link);
    EXPECT_EQ(runs[1].text, "the docs");
    EXPECT_EQ(runs[1].url, "https://example.org");
    EXPECT_EQ(runs[0].kind, MdInlineKind::Text);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, InlineImage) {
    const auto doc = parseMarkdown("before ![a cat](https://e.org/cat.png) after");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[0].kind, MdInlineKind::Text);
    EXPECT_EQ(runs[1].kind, MdInlineKind::Image);
    EXPECT_EQ(runs[1].text, "a cat");
    EXPECT_EQ(runs[1].url, "https://e.org/cat.png");
    EXPECT_EQ(runs[2].kind, MdInlineKind::Text);
}

TEST_F(MarkdownTests, ImageWithEmptyAlt) {
    const auto doc = parseMarkdown("![](https://e.org/x.png)");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 1U);
    EXPECT_EQ(runs[0].kind, MdInlineKind::Image);
    EXPECT_TRUE(runs[0].text.empty());
    EXPECT_EQ(runs[0].url, "https://e.org/x.png");
}

TEST_F(MarkdownTests, ImageAltFlattensNestedFormatting) {
    // md4c emits nested spans inside the alt; the model flattens them
    // to plain text — image alt text never carries styling.
    const auto doc = parseMarkdown("![**bold** cat](https://e.org/c.png)");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 1U);
    EXPECT_EQ(runs[0].kind, MdInlineKind::Image);
    EXPECT_EQ(runs[0].text, "bold cat");
    EXPECT_EQ(runs[0].url, "https://e.org/c.png");
}

// ---------------------------------------------------------------------------
// Breaks
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, SoftBreakInsideParagraph) {
    const auto doc = parseMarkdown("line one\nline two");
    ASSERT_EQ(doc.blocks.size(), 1U);
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::SoftBreak);
}

TEST_F(MarkdownTests, HardBreakInsideParagraph) {
    const auto doc = parseMarkdown("line one  \nline two");
    const auto& runs = blockInlines(*doc.blocks[0]);
    ASSERT_EQ(runs.size(), 3U);
    EXPECT_EQ(runs[1].kind, MdInlineKind::HardBreak);
}

// ---------------------------------------------------------------------------
// Block quotes / rules
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, BlockQuoteDepth) {
    const auto doc = parseMarkdown("> quoted text");
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->quoteDepth, 1);
    EXPECT_EQ(plainText(*doc.blocks[0]), "quoted text");
}

TEST_F(MarkdownTests, HorizontalRule) {
    const auto doc = parseMarkdown("above\n\n---\n\nbelow");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_EQ(doc.blocks[1]->kind, MdBlockKind::Rule);
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, NamedEntitiesDecoded) {
    const auto doc = parseMarkdown("a &amp; b &lt; c");
    EXPECT_EQ(plainText(*doc.blocks[0]), "a & b < c");
}

// ---------------------------------------------------------------------------
// Mixed document
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, ProseThenCodeThenProse) {
    const auto doc = parseMarkdown("Here is code:\n\n```fb\nPrint 1\n```\n\nDone.");
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Paragraph);
    EXPECT_EQ(doc.blocks[1]->kind, MdBlockKind::CodeFence);
    EXPECT_EQ(doc.blocks[2]->kind, MdBlockKind::Paragraph);
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
    const auto& table = doc.blocks[0]->as<MdTable>();
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
    const auto& table = doc.blocks[0]->as<MdTable>();
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
    const auto& table = doc.blocks[0]->as<MdTable>();
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
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Paragraph);
}

// ---------------------------------------------------------------------------
// Patch proposals (SEARCH/REPLACE)
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, PatchBlockBasic) {
    const auto doc = parseMarkdown(
        "<<<<<<< SEARCH\n"
        "old line\n"
        "=======\n"
        "new line\n"
        ">>>>>>> REPLACE\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Patch);
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchSearch, "old line\n");
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchReplace, "new line\n");
    EXPECT_TRUE(doc.blocks[0]->as<MdPatch>().patchTarget.empty());
}

TEST_F(MarkdownTests, PatchBlockCarriesTargetPath) {
    const auto doc = parseMarkdown(
        "<<<<<<< SEARCH path/to/file.bas\n"
        "x\n"
        "=======\n"
        "y\n"
        ">>>>>>> REPLACE\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Patch);
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchTarget, "path/to/file.bas");
}

TEST_F(MarkdownTests, PatchBlockBetweenMarkdownSegments) {
    const auto doc = parseMarkdown(
        "Before.\n\n"
        "<<<<<<< SEARCH\n"
        "a\n"
        "=======\n"
        "b\n"
        ">>>>>>> REPLACE\n\n"
        "After."
    );
    ASSERT_EQ(doc.blocks.size(), 3U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Paragraph);
    EXPECT_EQ(plainText(*doc.blocks[0]), "Before.");
    EXPECT_EQ(doc.blocks[1]->kind, MdBlockKind::Patch);
    EXPECT_EQ(doc.blocks[1]->as<MdPatch>().patchSearch, "a\n");
    EXPECT_EQ(doc.blocks[1]->as<MdPatch>().patchReplace, "b\n");
    EXPECT_EQ(doc.blocks[2]->kind, MdBlockKind::Paragraph);
    EXPECT_EQ(plainText(*doc.blocks[2]), "After.");
}

TEST_F(MarkdownTests, PartialPatchAtEndIsDropped) {
    // Mid-stream — the closing marker has not arrived yet. The block
    // is silently consumed so md4c never sees the `=======` and treats
    // the preceding text as an H2 setext heading.
    const auto doc = parseMarkdown(
        "Lead-in.\n\n"
        "<<<<<<< SEARCH\n"
        "in progress\n"
        "=======\n"
        "partial..."
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Paragraph);
    EXPECT_EQ(plainText(*doc.blocks[0]), "Lead-in.");
}

TEST_F(MarkdownTests, MultiLineSearchAndReplace) {
    const auto doc = parseMarkdown(
        "<<<<<<< SEARCH\n"
        "line one\n"
        "line two\n"
        "=======\n"
        "replaced one\n"
        "replaced two\n"
        "replaced three\n"
        ">>>>>>> REPLACE\n"
    );
    ASSERT_EQ(doc.blocks.size(), 1U);
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchSearch, "line one\nline two\n");
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchReplace, "replaced one\nreplaced two\nreplaced three\n");
}

TEST_F(MarkdownTests, MultiplePatchBlocks) {
    const auto doc = parseMarkdown(
        "<<<<<<< SEARCH\n"
        "a\n"
        "=======\n"
        "A\n"
        ">>>>>>> REPLACE\n"
        "<<<<<<< SEARCH\n"
        "b\n"
        "=======\n"
        "B\n"
        ">>>>>>> REPLACE\n"
    );
    ASSERT_EQ(doc.blocks.size(), 2U);
    EXPECT_EQ(doc.blocks[0]->kind, MdBlockKind::Patch);
    EXPECT_EQ(doc.blocks[0]->as<MdPatch>().patchSearch, "a\n");
    EXPECT_EQ(doc.blocks[1]->kind, MdBlockKind::Patch);
    EXPECT_EQ(doc.blocks[1]->as<MdPatch>().patchSearch, "b\n");
}

// ---------------------------------------------------------------------------
// Code block resolver — slow-path lookup so the laid-out doc can drop its
// duplicated copy of every snippet's body.
// ---------------------------------------------------------------------------

TEST_F(MarkdownTests, ResolveCodeBlockTextReturnsFenceBody) {
    const wxString markdown = "```\nPrint 1\nPrint 2\n```";
    EXPECT_EQ(resolveCodeBlockText(markdown, 0), "Print 1\nPrint 2\n");
}

TEST_F(MarkdownTests, ResolveCodeBlockTextRespectsIndex) {
    const wxString markdown = "first prose\n\n"
                              "```\nfirst body\n```\n\n"
                              "middle prose\n\n"
                              "```\nsecond body\n```\n";
    EXPECT_EQ(resolveCodeBlockText(markdown, 0), "first body\n");
    EXPECT_EQ(resolveCodeBlockText(markdown, 1), "second body\n");
}

TEST_F(MarkdownTests, ResolveCodeBlockTextReturnsEmptyForOutOfRange) {
    const wxString markdown = "```\nbody\n```";
    EXPECT_EQ(resolveCodeBlockText(markdown, 1), "");
    EXPECT_EQ(resolveCodeBlockText("no code here", 0), "");
}

TEST_F(MarkdownTests, ResolveCodeBlockTextWorksOnUnterminatedFence) {
    // Mid-stream — the chat view may ask for the body of a fence whose
    // closing ``` hasn't arrived yet. Should still return what's there.
    const wxString markdown = "```\npartial\nmore";
    EXPECT_EQ(resolveCodeBlockText(markdown, 0), "partial\nmore\n");
}
