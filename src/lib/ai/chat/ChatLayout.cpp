//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ChatLayout.hpp"
#include <optional>
using namespace fbide;

namespace {

// Vertical gap between consecutive blocks.
constexpr int kBlockGap = 8;
// Padding inside the code-block background — above, below and left of text.
constexpr int kCodePadding = 8;
// Left indent added per block-quote nesting level.
constexpr int kQuoteIndent = 16;
// Left indent added per list nesting level.
constexpr int kListIndent = 24;
// Height of a horizontal rule line.
constexpr int kRuleHeight = 13;

/// Body-relative point-size bump for heading level 1-6.
auto headingSizeDelta(const unsigned level) -> int {
    switch (level) {
    case 1:
        return 10;
    case 2:
        return 6;
    case 3:
        return 4;
    case 4:
        return 2;
    case 5:
        return 1;
    default:
        return 0;
    }
}

/// Map markdown inline styling to a layout text style.
auto textStyleOf(const MdStyle& style) -> TextStyle {
    return {
        .sizeDelta = 0,
        .bold = style.bold,
        .italic = style.italic,
        .underline = false,
        .strikethrough = style.strikethrough,
        .monospace = style.code,
    };
}

/// One unit of wrappable inline content.
struct WrapItem {
    enum class Type : std::uint8_t { Word,
        Space,
        HardBreak };
    Type type = Type::Word;
    wxString text;   ///< Word text (Word only).
    TextStyle style; ///< Font selection (Word / Space).
    wxColour colour; ///< Foreground colour (Word).
    int linkId = -1; ///< Link index, or -1.
};

/// Split `text` into Word items separated by collapsed Space items, appending
/// them to `items`. Leading/trailing whitespace yields edge Space items, which
/// the wrapper drops at line boundaries.
void appendWords(
    std::vector<WrapItem>& items,
    const wxString& text,
    const TextStyle& style,
    const wxColour& colour,
    const int linkId
) {
    wxString word;
    bool sawSpace = false;
    const auto flushWord = [&] {
        if (!word.empty()) {
            items.push_back({ .type = WrapItem::Type::Word,
                .text = word,
                .style = style,
                .colour = colour,
                .linkId = linkId });
            word.clear();
        }
    };
    for (const wxUniChar ch : text) {
        if (ch == ' ' || ch == '\t') {
            flushWord();
            sawSpace = true;
        } else {
            if (sawSpace) {
                items.push_back({ .type = WrapItem::Type::Space, .style = style });
                sawSpace = false;
            }
            word += ch;
        }
    }
    flushWord();
    if (sawSpace) {
        items.push_back({ .type = WrapItem::Type::Space, .style = style });
    }
}

/// Carries the layout state while walking the document's blocks.
struct Engine {
    const MdDoc& doc;
    const int width;
    const TextMeasurer& measurer;
    const ChatPalette& palette;
    const CodeFenceHighlighter& highlightFence;

    LaidOutDoc out;
    int y = 0; ///< Running vertical cursor.

    /// Insert the inter-block gap (skipped before the first block).
    void blockGap() {
        if (y > 0) {
            y += kBlockGap;
        }
    }

    /// Flatten a block's inline fragments into wrappable items. `baseSizeDelta`
    /// and `baseBold` apply the heading font on top of inline styling. Link
    /// runs are registered in `out.links` and tagged with their id.
    auto flatten(const std::vector<MdInline>& inlines, const int baseSizeDelta, const bool baseBold)
        -> std::vector<WrapItem> {
        std::vector<WrapItem> items;
        for (const auto& inl : inlines) {
            if (inl.kind == MdInlineKind::HardBreak) {
                items.push_back({ .type = WrapItem::Type::HardBreak });
                continue;
            }
            if (inl.kind == MdInlineKind::SoftBreak) {
                items.push_back({ .type = WrapItem::Type::Space,
                    .style = { .sizeDelta = baseSizeDelta, .bold = baseBold } });
                continue;
            }
            TextStyle style = textStyleOf(inl.style);
            style.sizeDelta = baseSizeDelta;
            style.bold = style.bold || baseBold;

            wxColour colour = palette.text;
            int linkId = -1;
            if (inl.kind == MdInlineKind::Link) {
                style.underline = true;
                colour = palette.link;
                if (!inl.url.empty()) {
                    out.links.push_back({ .url = inl.url });
                    linkId = static_cast<int>(out.links.size()) - 1;
                }
            }
            appendWords(items, inl.text, style, colour, linkId);
        }
        return items;
    }

    /// Greedily wrap `items` into prose lines starting at `contentLeft`. An
    /// optional `marker` run is placed on the first line (list bullets).
    void emitWrapped(
        const std::vector<WrapItem>& items,
        const int contentLeft,
        const std::optional<PaintRun>& marker,
        const int quoteDepth
    ) {
        const int bodyHeight = measurer.lineHeight(TextStyle {});
        std::vector<PaintRun> lineRuns;
        int x = contentLeft;
        int maxHeight = 0;
        bool pendingSpace = false;
        TextStyle spaceStyle;
        bool firstLine = true;
        bool produced = false;

        const auto flush = [&] {
            PaintLine line;
            line.kind = LineKind::Prose;
            line.y = y;
            line.height = maxHeight > 0 ? maxHeight : bodyHeight;
            line.quoteDepth = quoteDepth;
            if (firstLine && marker.has_value()) {
                line.runs.push_back(*marker);
            }
            for (auto& run : lineRuns) {
                line.runs.push_back(std::move(run));
            }
            y += line.height;
            out.lines.push_back(std::move(line));
            lineRuns.clear();
            x = contentLeft;
            maxHeight = 0;
            pendingSpace = false;
            firstLine = false;
            produced = true;
        };

        for (const auto& item : items) {
            if (item.type == WrapItem::Type::HardBreak) {
                flush();
                continue;
            }
            if (item.type == WrapItem::Type::Space) {
                pendingSpace = true;
                spaceStyle = item.style;
                continue;
            }
            const int wordWidth = measurer.width(item.text, item.style);
            const int spaceWidth = pendingSpace ? measurer.width(" ", spaceStyle) : 0;
            if (!lineRuns.empty() && x + spaceWidth + wordWidth > width) {
                flush(); // wrap — the leading space is dropped
            } else if (pendingSpace && !lineRuns.empty()) {
                x += spaceWidth;
            }
            pendingSpace = false;
            lineRuns.push_back({ .text = item.text,
                .style = item.style,
                .colour = item.colour,
                .x = x,
                .width = wordWidth,
                .linkId = item.linkId });
            x += wordWidth;
            maxHeight = std::max(maxHeight, measurer.lineHeight(item.style));
        }
        if (!lineRuns.empty() || (!produced && marker.has_value())) {
            flush();
        }
    }

    /// Build the bullet / number run that prefixes a list item.
    auto makeMarker(const MdBlock& block, const int contentLeft) -> PaintRun {
        const wxString text = block.listOrdered
                                ? wxString::Format("%d. ", block.listOrdinal)
                                : wxString(wxUniChar(0x2022)) + " ";
        const int markerWidth = measurer.width(text, TextStyle {});
        return { .text = text,
            .style = {},
            .colour = palette.text,
            .x = std::max(0, contentLeft - markerWidth),
            .width = markerWidth,
            .linkId = -1 };
    }

    /// Emit a fenced code block: a padded background strip carrying one
    /// PaintLine per highlighted code line. Code lines are not wrapped.
    void emitCode(const MdBlock& block) {
        blockGap();
        const int left = block.quoteDepth * kQuoteIndent;
        const TextStyle mono { .monospace = true };
        const int lineHeight = measurer.lineHeight(mono);
        const auto codeLines = highlightFence(block.codeText, block.codeLang);

        // Top padding strip — an empty Code line the painter fills with the
        // code background.
        out.lines.push_back({ .kind = LineKind::Code,
            .y = y,
            .height = kCodePadding,
            .quoteDepth = block.quoteDepth,
            .runs = {} });
        y += kCodePadding;

        for (const auto& codeLine : codeLines) {
            PaintLine line;
            line.kind = LineKind::Code;
            line.y = y;
            line.height = lineHeight;
            line.quoteDepth = block.quoteDepth;
            int x = left + kCodePadding;
            for (const auto& codeRun : codeLine) {
                const TextStyle style { .bold = codeRun.bold,
                    .italic = codeRun.italic,
                    .underline = codeRun.underlined,
                    .monospace = true };
                const int runWidth = measurer.width(codeRun.text, style);
                line.runs.push_back({ .text = codeRun.text,
                    .style = style,
                    .colour = codeRun.colour,
                    .x = x,
                    .width = runWidth,
                    .linkId = -1 });
                x += runWidth;
            }
            out.lines.push_back(std::move(line));
            y += lineHeight;
        }

        out.lines.push_back({ .kind = LineKind::Code,
            .y = y,
            .height = kCodePadding,
            .quoteDepth = block.quoteDepth,
            .runs = {} });
        y += kCodePadding;
    }

    /// Emit a horizontal rule line.
    void emitRule(const int quoteDepth) {
        blockGap();
        out.lines.push_back({ .kind = LineKind::Rule,
            .y = y,
            .height = kRuleHeight,
            .quoteDepth = quoteDepth,
            .runs = {} });
        y += kRuleHeight;
    }

    /// Lay out every block in document order.
    void run() {
        for (const auto& block : doc.blocks) {
            switch (block.kind) {
            case MdBlockKind::Paragraph:
                blockGap();
                emitWrapped(
                    flatten(block.inlines, 0, false),
                    block.quoteDepth * kQuoteIndent,
                    std::nullopt,
                    block.quoteDepth
                );
                break;
            case MdBlockKind::Heading:
                blockGap();
                emitWrapped(
                    flatten(block.inlines, headingSizeDelta(block.headingLevel), true),
                    block.quoteDepth * kQuoteIndent,
                    std::nullopt,
                    block.quoteDepth
                );
                break;
            case MdBlockKind::ListItem: {
                blockGap();
                const int left = block.quoteDepth * kQuoteIndent + block.listDepth * kListIndent;
                std::optional<PaintRun> marker;
                if (block.listMarker) {
                    marker = makeMarker(block, left);
                }
                emitWrapped(flatten(block.inlines, 0, false), left, marker, block.quoteDepth);
                break;
            }
            case MdBlockKind::CodeFence:
                emitCode(block);
                break;
            case MdBlockKind::Rule:
                emitRule(block.quoteDepth);
                break;
            }
        }
        out.width = width;
        out.height = y;
    }
};

} // namespace

auto fbide::layoutMarkdown(
    const MdDoc& doc,
    const int width,
    const TextMeasurer& measurer,
    const ChatPalette& palette,
    const CodeFenceHighlighter& highlightFence
) -> LaidOutDoc {
    Engine engine { .doc = doc,
        .width = width,
        .measurer = measurer,
        .palette = palette,
        .highlightFence = highlightFence };
    engine.run();
    return std::move(engine.out);
}
