//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "MarkdownTestFixtures.hpp"
#include "markdown/MarkdownDocument.hpp"

using namespace fbide;
using namespace fbide::tests;

namespace {

/// Standard / narrow widths used across the layout tests below. Named
/// so the tidy magic-number check doesn't trip on every `set(...)` call.
constexpr int kWideWidth = 500;
constexpr int kNarrowWidth = 80;

// GTest fixture in the anonymous namespace (internal linkage). The
// `protected` measurer is a deliberate GTest idiom — `TEST_F`-generated
// derived classes reach for it directly — so the otherwise-reasonable
// "non-public members should be private" check is suppressed.
class MarkdownDocumentTests : public testing::Test {
protected:
    FakeMeasurer measurer; // NOLINT(*-non-private-member-variables-in-classes)

    auto set(MarkdownDocument& doc, const wxString& markdown, const int width) -> bool {
        return doc.setMarkdown(markdown, width, measurer, fakePalette(), splitHighlight);
    }
};

} // namespace

TEST_F(MarkdownDocumentTests, EmptyByDefault) {
    const MarkdownDocument doc;
    EXPECT_TRUE(doc.markdown().empty());
    EXPECT_TRUE(doc.laid().lines.empty());
    EXPECT_EQ(doc.height(), 0);
}

TEST_F(MarkdownDocumentTests, SetMarkdownLaysOutAndReturnsTrue) {
    MarkdownDocument doc;
    EXPECT_TRUE(set(doc, "hello world", kWideWidth));
    EXPECT_EQ(doc.markdown(), "hello world");
    EXPECT_FALSE(doc.laid().lines.empty());
    EXPECT_GT(doc.height(), 0);
}

TEST_F(MarkdownDocumentTests, SetSameMarkdownAtSameWidthReturnsFalse) {
    MarkdownDocument doc;
    set(doc, "hello", kWideWidth);
    const int firstHeight = doc.height();
    EXPECT_FALSE(set(doc, "hello", kWideWidth)); // unchanged — no re-layout
    EXPECT_EQ(doc.height(), firstHeight);
}

TEST_F(MarkdownDocumentTests, SetMarkdownAtDifferentWidthRebuilds) {
    MarkdownDocument doc;
    const wxString text = "one two three four five six seven eight nine ten";
    set(doc, text, kWideWidth);
    const int wide = doc.height();
    EXPECT_TRUE(set(doc, text, kNarrowWidth)); // narrow → must wrap → re-lay
    EXPECT_GT(doc.height(), wide);
}

TEST_F(MarkdownDocumentTests, SetDifferentMarkdownRebuilds) {
    MarkdownDocument doc;
    set(doc, "first", kWideWidth);
    EXPECT_TRUE(set(doc, "second\n\nthird", kWideWidth));
    EXPECT_EQ(doc.markdown(), "second\n\nthird");
}

TEST_F(MarkdownDocumentTests, ClearResetsState) {
    MarkdownDocument doc;
    set(doc, "hello", kWideWidth);
    doc.clear();
    EXPECT_TRUE(doc.markdown().empty());
    EXPECT_TRUE(doc.laid().lines.empty());
    EXPECT_EQ(doc.height(), 0);
    // After clear, the same markdown is "new" again and rebuilds.
    EXPECT_TRUE(set(doc, "hello", kWideWidth));
}
