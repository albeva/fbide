//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "markdown/MarkdownDocument.hpp"

using namespace fbide;

namespace {

/// Deterministic measurer: every glyph is 10 px wide (8 px monospace),
/// line height tracks the size delta. Mirrors the helper in
/// `MarkdownLayoutTests` so the two suites share assumptions.
class FakeMeasurer final : public TextMeasurer {
public:
    auto width(const wxString& text, const TextStyle& style) const -> int override {
        return static_cast<int>(text.length()) * (style.monospace ? 8 : 10);
    }
    auto lineHeight(const TextStyle& style) const -> int override {
        return 20 + style.sizeDelta;
    }
};

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

} // namespace

class MarkdownDocumentTests : public testing::Test {
protected:
    FakeMeasurer measurer;

    auto set(MarkdownDocument& doc, const wxString& markdown, const int width) -> bool {
        return doc.setMarkdown(markdown, width, measurer, palette(), splitHighlight);
    }
};

TEST_F(MarkdownDocumentTests, EmptyByDefault) {
    const MarkdownDocument doc;
    EXPECT_TRUE(doc.markdown().empty());
    EXPECT_TRUE(doc.laid().lines.empty());
    EXPECT_EQ(doc.height(), 0);
}

TEST_F(MarkdownDocumentTests, SetMarkdownLaysOutAndReturnsTrue) {
    MarkdownDocument doc;
    EXPECT_TRUE(set(doc, "hello world", 500));
    EXPECT_EQ(doc.markdown(), "hello world");
    EXPECT_FALSE(doc.laid().lines.empty());
    EXPECT_GT(doc.height(), 0);
}

TEST_F(MarkdownDocumentTests, SetSameMarkdownAtSameWidthReturnsFalse) {
    MarkdownDocument doc;
    set(doc, "hello", 500);
    const int firstHeight = doc.height();
    EXPECT_FALSE(set(doc, "hello", 500)); // unchanged — no re-layout
    EXPECT_EQ(doc.height(), firstHeight);
}

TEST_F(MarkdownDocumentTests, SetMarkdownAtDifferentWidthRebuilds) {
    MarkdownDocument doc;
    const wxString text = "one two three four five six seven eight nine ten";
    set(doc, text, 500);
    const int wide = doc.height();
    EXPECT_TRUE(set(doc, text, 80)); // narrow → must wrap → re-lay
    EXPECT_GT(doc.height(), wide);
}

TEST_F(MarkdownDocumentTests, SetDifferentMarkdownRebuilds) {
    MarkdownDocument doc;
    set(doc, "first", 500);
    EXPECT_TRUE(set(doc, "second\n\nthird", 500));
    EXPECT_EQ(doc.markdown(), "second\n\nthird");
}

TEST_F(MarkdownDocumentTests, ClearResetsState) {
    MarkdownDocument doc;
    set(doc, "hello", 500);
    doc.clear();
    EXPECT_TRUE(doc.markdown().empty());
    EXPECT_TRUE(doc.laid().lines.empty());
    EXPECT_EQ(doc.height(), 0);
    // After clear, the same markdown is "new" again and rebuilds.
    EXPECT_TRUE(set(doc, "hello", 500));
}
