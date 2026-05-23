//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownLayout.hpp"
#include <optional>
using namespace fbide;

namespace {

// Vertical gap between consecutive blocks.
constexpr int kBlockGap = 8;
// Padding inside the code-block background — above, below and left of text.
constexpr int kCodePadding = 8;
// Continuation indent for soft-wrapped code lines, in characters.
constexpr int kCodeWrapIndent = 2;
// Left indent added per block-quote nesting level.
constexpr int kQuoteIndent = 16;
// Left indent added per list nesting level.
constexpr int kListIndent = 24;
// Height of a horizontal rule line.
constexpr int kRuleHeight = 13;

// Marker glyphs used by `makeMarker` / image placeholders. Spelled out as
// named constants so the tidy magic-number check doesn't flag them and so
// the meaning is obvious at the call site.
constexpr wchar_t kBulletGlyph = 0x2022;        // U+2022 BULLET — plain list item.
constexpr wchar_t kTaskUncheckedGlyph = 0x2610; // U+2610 BALLOT BOX.
constexpr wchar_t kTaskCheckedGlyph = 0x2611;   // U+2611 BALLOT BOX WITH CHECK.
constexpr int kImageGlyph = 0x1F5BC;            // U+1F5BC FRAME WITH PICTURE — image label.

/// True when `lang` (a fence tag) denotes FreeBASIC. Mirrors the same
/// rule in AiChatView so layout and paint agree on which fences get the
/// theme font vs. the system monospace font. Requires an explicit tag —
/// untagged fences are NOT assumed to be FreeBASIC.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
}

// Body-relative point-size bumps for `#` through `######`. Tapered so
// H1 reads as a title and H6 stays close to body weight.
constexpr int kH1SizeDelta = 10;
constexpr int kH2SizeDelta = 6;
constexpr int kH3SizeDelta = 4;
constexpr int kH4SizeDelta = 2;
constexpr int kH5SizeDelta = 1;

/// Body-relative point-size bump for heading level 1-6. Case labels
/// are the markdown heading levels (`#`..`######`); the magic-number
/// suppression covers `case 5:` which the check flags.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
auto headingSizeDelta(const unsigned level) -> int {
    switch (level) {
    case 1:
        return kH1SizeDelta;
    case 2:
        return kH2SizeDelta;
    case 3:
        return kH3SizeDelta;
    case 4:
        return kH4SizeDelta;
    case 5:
        return kH5SizeDelta;
    default:
        return 0;
    }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

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

// Forward declaration so the helper below can refer to `WrapItem`; the
// full type is defined just after.
struct WrapItem;

/// Placeholder label for an image used inside a table cell — table
/// cells stay text-only so the bitmap is replaced by its alt text
/// rendered as a clickable link.
[[nodiscard]] auto imageCellLabel(const WrapItem& item) -> wxString;

/// One unit of wrappable inline content.
struct WrapItem {
    enum class Type : std::uint8_t { Word,
        Space,
        HardBreak,
        Image };
    Type type = Type::Word;
    wxString text {};   ///< Word text (Word only). Alt text (Image).
    TextStyle style {}; ///< Font selection (Word / Space).
    wxColour colour {}; ///< Foreground colour (Word).
    int linkId = -1;    ///< Link index, or -1.

    /// Image-only payload. Packed in a struct so designated initialisers
    /// of non-image WrapItems don't have to enumerate every image field.
    struct ImagePayload {
        ImageInfo::State state = ImageInfo::State::Failed;
        wxBitmap bitmap;
        wxString url;
        int width = 0;  ///< Natural pixel width of the bitmap.
        int height = 0; ///< Natural pixel height.
    };
    ImagePayload image {};
};

auto imageCellLabel(const WrapItem& item) -> wxString {
    return wxString(wxUniChar(kImageGlyph)) + " "
         + (item.text.empty() ? wxString("image") : item.text);
}

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
    NO_COPY_AND_MOVE(Engine)

    const MdDoc& m_doc;
    const int m_width;
    const TextMeasurer& m_measurer;
    const MarkdownPalette& m_palette;
    const CodeFenceHighlighter& m_highlightFence;
    const ImageResolver& m_resolveImage;

    LaidOutDoc m_out {};
    int m_yPos = 0; ///< Running vertical cursor.

    Engine(
        const MdDoc& doc,
        const int width,
        const TextMeasurer& measurer,
        const MarkdownPalette& palette,
        const CodeFenceHighlighter& highlightFence,
        const ImageResolver& resolveImage
    )
    : m_doc(doc)
    , m_width(width)
    , m_measurer(measurer)
    , m_palette(palette)
    , m_highlightFence(highlightFence)
    , m_resolveImage(resolveImage) {}

    /// Insert the inter-block gap (skipped before the first block).
    void blockGap() {
        if (m_yPos > 0) {
            m_yPos += kBlockGap;
        }
    }

    /// Flatten a block's inline fragments into wrappable items. `baseSizeDelta`
    /// and `baseBold` apply the heading font on top of inline styling. Link
    /// runs are registered in `out.links` and tagged with their id. Image
    /// inlines produce an `Image` WrapItem the wrapper turns into a
    /// dedicated `LineKind::Image` line (or a placeholder when not ready).
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
            if (inl.kind == MdInlineKind::Image) {
                appendImage(items, inl);
                continue;
            }
            TextStyle style = textStyleOf(inl.style);
            style.sizeDelta = baseSizeDelta;
            style.bold = style.bold || baseBold;

            wxColour colour = m_palette.text;
            int linkId = -1;
            if (inl.kind == MdInlineKind::Link) {
                style.underline = true;
                colour = m_palette.link;
                if (!inl.url.empty()) {
                    m_out.links.push_back({ .url = inl.url });
                    linkId = static_cast<int>(m_out.links.size()) - 1;
                }
            }
            appendWords(items, inl.text, style, colour, linkId);
        }
        return items;
    }

    /// Resolve `inl` (an Image inline) and append a single WrapItem of type
    /// Image. The wrapper consumes it as a block-level event: flushing the
    /// current prose line, emitting either an Image PaintLine (Ready) or
    /// a placeholder prose line (Loading / Failed), then continuing.
    void appendImage(std::vector<WrapItem>& items, const MdInline& inl) {
        WrapItem item;
        item.type = WrapItem::Type::Image;
        item.text = inl.text;
        item.image.url = inl.url;
        if (m_resolveImage) {
            const ImageInfo info = m_resolveImage(inl.url);
            item.image.state = info.state;
            item.image.bitmap = info.bitmap;
            item.image.width = info.width;
            item.image.height = info.height;
        }
        if (!inl.url.empty()) {
            m_out.links.push_back({ .url = inl.url });
            item.linkId = static_cast<int>(m_out.links.size()) - 1;
        }
        items.push_back(std::move(item));
    }

    /// Render `item` (an Image WrapItem) onto its own PaintLine. Ready
    /// images become a `LineKind::Image` line scaled to fit the available
    /// width; Loading / Failed images become a placeholder prose line with
    /// a clickable link-styled label so the user can still open the URL.
    void emitImageLine(const WrapItem& item, const int contentLeft, const int quoteDepth) {
        if (item.image.state == ImageInfo::State::Ready && item.image.bitmap.IsOk()
            && item.image.width > 0 && item.image.height > 0) {
            const int avail = std::max(1, m_width - contentLeft);
            int drawWidth = item.image.width;
            int drawHeight = item.image.height;
            if (drawWidth > avail) {
                // Scale proportionally so the image fits the bubble width.
                drawHeight = std::max(1, drawHeight * avail / drawWidth);
                drawWidth = avail;
            }
            PaintLine line;
            line.kind = LineKind::Image;
            line.y = m_yPos;
            line.height = drawHeight;
            line.quoteDepth = quoteDepth;
            line.image.bitmap = item.image.bitmap;
            line.image.url = item.image.url;
            line.image.alt = item.text;
            line.image.drawWidth = drawWidth;
            line.image.drawHeight = drawHeight;
            line.image.x = contentLeft;
            // Hit-test region for click handling.
            line.runs.push_back({ .text = {},
                .style = {},
                .colour = m_palette.link,
                .x = contentLeft,
                .width = drawWidth,
                .linkId = item.linkId });
            m_yPos += drawHeight;
            m_out.lines.push_back(std::move(line));
            return;
        }

        // Placeholder — render as a single styled link run on its own line.
        const wxString label = wxString(wxUniChar(kImageGlyph)) + " "
                             + (item.text.empty() ? wxString("image") : item.text)
                             + (item.image.state == ImageInfo::State::Loading
                                     ? wxString(" (loading\xE2\x80\xA6)") // U+2026 horizontal ellipsis
                                     : wxString(" (failed)"));
        const TextStyle style { .underline = true };
        const int width = m_measurer.width(label, style);
        PaintLine line;
        line.kind = LineKind::Prose;
        line.y = m_yPos;
        line.height = m_measurer.lineHeight(style);
        line.quoteDepth = quoteDepth;
        line.runs.push_back({ .text = label,
            .style = style,
            .colour = m_palette.link,
            .x = contentLeft,
            .width = width,
            .linkId = item.linkId });
        m_yPos += line.height;
        m_out.lines.push_back(std::move(line));
    }

    /// Greedily wrap `items` into prose lines starting at `contentLeft`. An
    /// optional `marker` run is placed on the first line (list bullets).
    void emitWrapped(
        const std::vector<WrapItem>& items,
        const int contentLeft,
        const std::optional<PaintRun>& marker,
        const int quoteDepth
    ) {
        const int bodyHeight = m_measurer.lineHeight(TextStyle {});
        std::vector<PaintRun> lineRuns;
        int xPos = contentLeft;
        int maxHeight = 0;
        bool pendingSpace = false;
        TextStyle spaceStyle;
        bool firstLine = true;
        bool produced = false;

        const auto flush = [&] {
            PaintLine line;
            line.kind = LineKind::Prose;
            line.y = m_yPos;
            line.height = maxHeight > 0 ? maxHeight : bodyHeight;
            line.quoteDepth = quoteDepth;
            if (firstLine && marker.has_value()) {
                line.runs.push_back(*marker);
            }
            for (auto& run : lineRuns) {
                line.runs.push_back(std::move(run));
            }
            m_yPos += line.height;
            m_out.lines.push_back(std::move(line));
            lineRuns.clear();
            xPos = contentLeft;
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
            if (item.type == WrapItem::Type::Image) {
                // Block-level break: flush surrounding prose, then lay the
                // image (or a placeholder) on its own line. Subsequent
                // inline content continues on a fresh prose line.
                if (!lineRuns.empty()) {
                    flush();
                }
                if (firstLine && marker.has_value()) {
                    // The list marker has no host line yet — emit a stub
                    // prose line carrying it so the marker stays visible.
                    PaintLine stub;
                    stub.kind = LineKind::Prose;
                    stub.y = m_yPos;
                    stub.height = bodyHeight;
                    stub.quoteDepth = quoteDepth;
                    stub.runs.push_back(*marker);
                    m_yPos += stub.height;
                    m_out.lines.push_back(std::move(stub));
                }
                emitImageLine(item, contentLeft, quoteDepth);
                pendingSpace = false;
                firstLine = false;
                produced = true;
                continue;
            }
            const int wordWidth = m_measurer.width(item.text, item.style);
            const int spaceWidth = pendingSpace ? m_measurer.width(" ", spaceStyle) : 0;
            if (!lineRuns.empty() && xPos + spaceWidth + wordWidth > m_width) {
                flush(); // wrap — the leading space is dropped
            } else if (pendingSpace && !lineRuns.empty()) {
                xPos += spaceWidth;
            }
            pendingSpace = false;
            lineRuns.push_back({ .text = item.text,
                .style = item.style,
                .colour = item.colour,
                .x = xPos,
                .width = wordWidth,
                .linkId = item.linkId });
            xPos += wordWidth;
            maxHeight = std::max(maxHeight, m_measurer.lineHeight(item.style));
        }
        if (!lineRuns.empty() || (!produced && marker.has_value())) {
            flush();
        }
    }

    /// Build the bullet / number / checkbox run that prefixes a list item.
    auto makeMarker(const MdBlock& block, const int contentLeft) -> PaintRun {
        wxString text;
        if (block.isTask) {
            // BMP glyphs — render without needing an emoji font.
            text = wxString(wxUniChar(block.taskChecked ? kTaskCheckedGlyph : kTaskUncheckedGlyph)) + " ";
        } else if (block.listOrdered) {
            text = wxString::Format("%d. ", block.listOrdinal);
        } else {
            text = wxString(wxUniChar(kBulletGlyph)) + " ";
        }
        const int markerWidth = m_measurer.width(text, TextStyle {});
        return { .text = text,
            .style = {},
            .colour = m_palette.text,
            .x = std::max(0, contentLeft - markerWidth),
            .width = markerWidth,
            .linkId = -1 };
    }

    /// Soft-wrap one highlighted code line into one or more PaintLines of
    /// `kind` (Code for fences, PatchSearch/PatchReplace for proposals).
    /// Wrapping prefers token boundaries; a token wider than the available
    /// width is split between characters (the font is monospace). Wrapped
    /// continuation lines start at `contX`.
    void emitCodeLine(
        const CodeLine& codeLine,
        const LineKind kind,
        const int firstX,
        const int contX,
        const int rightEdge,
        const int lineHeight,
        const int charWidth,
        const int quoteDepth,
        const bool themed
    ) {
        PaintLine line;
        line.kind = kind;
        line.y = m_yPos;
        line.height = lineHeight;
        line.quoteDepth = quoteDepth;
        int fx = firstX;
        bool lineEmpty = true;

        const auto wrap = [&] {
            m_out.lines.push_back(std::move(line));
            m_yPos += lineHeight;
            line = PaintLine {};
            line.kind = kind;
            line.y = m_yPos;
            line.height = lineHeight;
            line.quoteDepth = quoteDepth;
            fx = contX;
            lineEmpty = true;
        };

        for (const auto& codeRun : codeLine) {
            const TextStyle style { .bold = codeRun.bold,
                .italic = codeRun.italic,
                .underline = codeRun.underlined,
                .monospace = true,
                .themed = themed };
            // Wrap before the run if the whole token would overflow — keeps
            // breaks on token boundaries where possible.
            if (!lineEmpty && fx + m_measurer.width(codeRun.text, style) > rightEdge) {
                wrap();
            }
            const wxString& text = codeRun.text;
            std::size_t pos = 0;
            while (pos < text.length()) {
                const int avail = rightEdge - fx;
                const std::size_t remaining = text.length() - pos;
                std::size_t take = avail > 0 ? static_cast<std::size_t>(avail / charWidth) : 0;
                if (take >= remaining) {
                    take = remaining;
                } else if (take == 0) {
                    if (!lineEmpty) {
                        wrap(); // no room — try a fresh line
                        continue;
                    }
                    take = 1; // fresh line still too narrow — force progress
                }
                const int segmentWidth = static_cast<int>(take) * charWidth;
                line.runs.push_back({ .text = text.Mid(pos, take),
                    .style = style,
                    .colour = codeRun.colour,
                    .x = fx,
                    .width = segmentWidth,
                    .linkId = -1 });
                fx += segmentWidth;
                pos += take;
                lineEmpty = false;
                if (pos < text.length()) {
                    wrap(); // the token spills onto the next line
                }
            }
        }
        m_out.lines.push_back(std::move(line));
        m_yPos += lineHeight;
    }

    /// Emit a fenced code block: a padded background strip carrying the
    /// highlighted code, soft-wrapped to the available width.
    void emitCode(const MdBlock& block) {
        blockGap();
        const int blockTop = m_yPos;
        const int left = block.quoteDepth * kQuoteIndent;
        const bool themed = isFreeBasicTag(block.codeLang);
        const TextStyle mono { .monospace = true, .themed = themed };
        const int lineHeight = m_measurer.lineHeight(mono);
        const int charWidth = std::max(1, m_measurer.width("0", mono));
        const auto codeLines = m_highlightFence(block.codeText, block.codeLang);

        const int codeLeft = left + kCodePadding;
        const int rightEdge = std::max(codeLeft + charWidth, m_width - kCodePadding);
        const int contX = codeLeft + (kCodeWrapIndent * charWidth);

        // Top padding strip — an empty Code line the painter fills with the
        // code background.
        m_out.lines.push_back({ .kind = LineKind::Code,
            .y = m_yPos,
            .height = kCodePadding,
            .quoteDepth = block.quoteDepth,
            .runs = {} });
        m_yPos += kCodePadding;

        for (const auto& codeLine : codeLines) {
            emitCodeLine(
                codeLine, LineKind::Code, codeLeft, contX, rightEdge,
                lineHeight, charWidth, block.quoteDepth, themed
            );
        }

        m_out.lines.push_back({ .kind = LineKind::Code,
            .y = m_yPos,
            .height = kCodePadding,
            .quoteDepth = block.quoteDepth,
            .runs = {} });
        m_yPos += kCodePadding;

        m_out.codeBlocks.push_back({ .y = blockTop, .height = m_yPos - blockTop });
    }

    /// Split `text` (verbatim, '\n'-separated) into one CodeLine per source
    /// line, all coloured `colour` and styled monospace. A trailing
    /// newline does NOT produce a final empty line — patch text typically
    /// ends with `\n` and we don't want a phantom blank row.
    [[nodiscard]] static auto toCodeLines(const wxString& text, const wxColour& colour)
        -> std::vector<CodeLine> {
        std::vector<CodeLine> out;
        wxString current;
        const auto flush = [&] {
            CodeLine line;
            if (!current.empty()) {
                line.push_back({ .text = current, .colour = colour });
            }
            out.push_back(std::move(line));
            current.clear();
        };
        for (const wxUniChar ch : text) {
            if (ch == '\n') {
                flush();
            } else {
                current += ch;
            }
        }
        if (!current.empty()) {
            flush();
        }
        return out;
    }

    /// Emit a SEARCH/REPLACE proposal: two stacked tinted strips holding
    /// the verbatim SEARCH (red) then REPLACE (green) lines. Soft-wrap
    /// reuses `emitCodeLine` with PatchSearch / PatchReplace kinds; the
    /// painter draws the tint band on the line background.
    void emitPatch(const MdBlock& block) {
        blockGap();
        const int blockTop = m_yPos;
        const int left = block.quoteDepth * kQuoteIndent;
        const TextStyle mono { .monospace = true };
        const int lineHeight = m_measurer.lineHeight(mono);
        const int charWidth = std::max(1, m_measurer.width("0", mono));

        const int codeLeft = left + kCodePadding;
        const int rightEdge = std::max(codeLeft + charWidth, m_width - kCodePadding);
        const int contX = codeLeft + (kCodeWrapIndent * charWidth);

        const auto emitStrip = [&](const wxString& text, const LineKind kind) {
            m_out.lines.push_back({ .kind = kind,
                .y = m_yPos,
                .height = kCodePadding,
                .quoteDepth = block.quoteDepth,
                .runs = {} });
            m_yPos += kCodePadding;
            for (const auto& codeLine : toCodeLines(text, m_palette.patchFg)) {
                emitCodeLine(
                    codeLine, kind, codeLeft, contX, rightEdge,
                    lineHeight, charWidth, block.quoteDepth, false
                );
            }
            m_out.lines.push_back({ .kind = kind,
                .y = m_yPos,
                .height = kCodePadding,
                .quoteDepth = block.quoteDepth,
                .runs = {} });
            m_yPos += kCodePadding;
        };

        emitStrip(block.patchSearch, LineKind::PatchSearch);
        emitStrip(block.patchReplace, LineKind::PatchReplace);

        m_out.patchBlocks.push_back({ .target = block.patchTarget,
            .search = block.patchSearch,
            .replace = block.patchReplace,
            .y = blockTop,
            .height = m_yPos - blockTop });
    }

    /// Emit a horizontal rule line.
    void emitRule(const int quoteDepth) {
        blockGap();
        m_out.lines.push_back({ .kind = LineKind::Rule,
            .y = m_yPos,
            .height = kRuleHeight,
            .quoteDepth = quoteDepth,
            .runs = {} });
        m_yPos += kRuleHeight;
    }

    /// One wrapped line of a single cell — runs with x offsets relative
    /// to the cell's left edge, plus the line's content width for
    /// alignment.
    struct WrappedCellLine {
        std::vector<PaintRun> runs;
        int width = 0;
    };

    /// Natural width of a cell laid out on one line, without wrapping.
    /// Used to seed the column-width allocation: each column's natural
    /// width is the max natural width across its cells.
    [[nodiscard]] auto measureNaturalWidth(const std::vector<WrapItem>& items) const -> int {
        int total = 0;
        bool sawNonSpace = false;
        for (const auto& item : items) {
            switch (item.type) {
            case WrapItem::Type::HardBreak:
                break; // single-line measurement ignores hard breaks
            case WrapItem::Type::Space:
                if (sawNonSpace) {
                    total += m_measurer.width(" ", item.style);
                }
                break;
            case WrapItem::Type::Word:
                total += m_measurer.width(item.text, item.style);
                sawNonSpace = true;
                break;
            case WrapItem::Type::Image:
                // Table cells degrade images to a labelled link; the
                // bitmap is not laid out inside a cell.
                total += m_measurer.width(imageCellLabel(item), TextStyle { .underline = true });
                sawNonSpace = true;
                break;
            }
        }
        return total;
    }

    /// Greedy-wrap a cell's WrapItems to `columnWidth`. Returns one
    /// `WrappedCellLine` per visual line. Always returns at least one
    /// entry — an empty cell yields one empty line so the row stays
    /// at the minimum height.
    [[nodiscard]] auto wrapCellToColumn(const std::vector<WrapItem>& items, const int columnWidth) const
        -> std::vector<WrappedCellLine> {
        std::vector<WrappedCellLine> lines;
        WrappedCellLine current;
        int xPos = 0;
        bool pendingSpace = false;
        TextStyle spaceStyle;

        const auto flush = [&] {
            current.width = xPos;
            lines.push_back(std::move(current));
            current = WrappedCellLine {};
            xPos = 0;
            pendingSpace = false;
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
            // Synthesise a plain word/style for an Image inline so the
            // table-cell path can wrap it like any other text token.
            wxString runText;
            TextStyle runStyle;
            wxColour runColour;
            if (item.type == WrapItem::Type::Image) {
                runText = imageCellLabel(item);
                runStyle = TextStyle { .underline = true };
                runColour = m_palette.link;
            } else {
                runText = item.text;
                runStyle = item.style;
                runColour = item.colour;
            }
            const int wordWidth = m_measurer.width(runText, runStyle);
            const int spaceWidth = pendingSpace ? m_measurer.width(" ", spaceStyle) : 0;
            if (!current.runs.empty() && xPos + spaceWidth + wordWidth > columnWidth) {
                flush();
            } else if (pendingSpace && !current.runs.empty()) {
                xPos += spaceWidth;
            }
            pendingSpace = false;
            current.runs.push_back({ .text = runText,
                .style = runStyle,
                .colour = runColour,
                .x = xPos,
                .width = wordWidth,
                .linkId = item.linkId });
            xPos += wordWidth;
        }
        if (!current.runs.empty()) {
            flush();
        }
        if (lines.empty()) {
            lines.push_back({}); // keep empty cells producing one line
        }
        return lines;
    }

    /// Emit a GFM pipe table. Algorithm:
    ///   1. Flatten each cell into WrapItems.
    ///   2. Natural width per column = max(cell natural width). Sum
    ///      compared to available width; if over budget, shrink columns
    ///      proportionally to a per-column minimum.
    ///   3. Per row, wrap each cell to its allocated column width.
    ///   4. Row height = max cell line count * line height. Emit one
    ///      PaintLine per visual line of the row, merging runs from
    ///      every cell into a single line with `tableColumns` set.
    void emitTable(const MdBlock& block) {
        blockGap();
        if (block.rows.empty() || block.rows.front().cells.empty()) {
            return;
        }
        const std::size_t columnCount = block.rows.front().cells.size();
        const int left = block.quoteDepth * kQuoteIndent;
        // Tables align with prose at the left edge — no extra padding
        // strip like code blocks have. Available width is the bubble
        // content width minus any block-quote indent.
        constexpr int kMinTableWidth = 80;
        const int available = std::max(kMinTableWidth, m_width - left);

        // Flatten every cell up front; reused for both natural-width
        // measurement and per-row wrapping.
        std::vector<std::vector<std::vector<WrapItem>>> cellItems(block.rows.size());
        for (std::size_t rowIdx = 0; rowIdx < block.rows.size(); rowIdx++) {
            cellItems.at(rowIdx).resize(columnCount);
            const auto& row = block.rows.at(rowIdx);
            for (std::size_t col = 0; col < row.cells.size() && col < columnCount; col++) {
                cellItems.at(rowIdx).at(col) = flatten(row.cells.at(col).inlines, 0, false);
            }
        }

        // Column natural widths.
        std::vector<int> naturalWidths(columnCount, 0);
        for (const auto& row : cellItems) {
            for (std::size_t col = 0; col < columnCount; col++) {
                naturalWidths.at(col) = std::max(naturalWidths.at(col), measureNaturalWidth(row.at(col)));
            }
        }

        // Allocate column widths. Pad each natural width with a small
        // gutter so adjacent columns aren't visually touching.
        constexpr int kColGutter = 12;
        constexpr int kMinColWidth = 40;
        int totalNatural = 0;
        for (const int width : naturalWidths) {
            totalNatural += width + kColGutter;
        }
        std::vector<int> columnWidths(columnCount, 0);
        if (totalNatural <= available) {
            for (std::size_t col = 0; col < columnCount; col++) {
                columnWidths.at(col) = naturalWidths.at(col) + kColGutter;
            }
        } else {
            const int minCol = std::max(kMinColWidth, m_measurer.width("XXX", TextStyle {}));
            for (std::size_t col = 0; col < columnCount; col++) {
                const int proportional = ((naturalWidths.at(col) + kColGutter) * available) / totalNatural;
                columnWidths.at(col) = std::max(minCol, proportional);
            }
        }

        // Column x positions — first column starts at the block's left
        // edge, sharing it with prose.
        std::vector<TableColumn> columns(columnCount);
        int xPos = left;
        for (std::size_t col = 0; col < columnCount; col++) {
            columns.at(col) = { .x = xPos, .width = columnWidths.at(col) };
            xPos += columnWidths.at(col);
        }

        // Wrap and emit each row.
        const TextStyle proseStyle;
        const int lineHeight = m_measurer.lineHeight(proseStyle);

        const std::size_t firstLineIndex = m_out.lines.size();
        for (std::size_t rowIdx = 0; rowIdx < block.rows.size(); rowIdx++) {
            const bool isHeader = rowIdx < block.headerRowCount;
            const LineKind kind = isHeader ? LineKind::TableHeader : LineKind::TableBody;

            std::vector<std::vector<WrappedCellLine>> cellLines(columnCount);
            std::size_t maxLines = 1;
            for (std::size_t col = 0; col < columnCount; col++) {
                cellLines.at(col) = wrapCellToColumn(cellItems.at(rowIdx).at(col), columnWidths.at(col) - kColGutter);
                maxLines = std::max(maxLines, cellLines.at(col).size());
            }

            for (std::size_t li = 0; li < maxLines; li++) {
                PaintLine line;
                line.kind = kind;
                line.y = m_yPos;
                line.height = lineHeight;
                line.quoteDepth = block.quoteDepth;
                line.tableColumns = columns;
                line.tableRowStart = (li == 0);

                for (std::size_t col = 0; col < columnCount; col++) {
                    if (li >= cellLines.at(col).size()) {
                        continue;
                    }
                    const auto& wrapped = cellLines.at(col).at(li);
                    const auto alignment = col < block.columnAlignment.size()
                                             ? block.columnAlignment.at(col)
                                             : MdTableAlignment::Default;
                    const int innerWidth = columnWidths.at(col) - kColGutter;
                    int alignOffset = (kColGutter / 2);
                    if (alignment == MdTableAlignment::Center) {
                        alignOffset += std::max(0, (innerWidth - wrapped.width) / 2);
                    } else if (alignment == MdTableAlignment::Right) {
                        alignOffset += std::max(0, innerWidth - wrapped.width);
                    }
                    const int cellXOffset = columns.at(col).x + alignOffset;
                    for (auto run : wrapped.runs) {
                        run.x += cellXOffset;
                        line.runs.push_back(std::move(run));
                    }
                }

                m_yPos += lineHeight;
                m_out.lines.push_back(std::move(line));
            }
        }
        // The last pushed line carries the bottom-border flag, so the
        // painter knows where to draw the closing rule.
        if (m_out.lines.size() > firstLineIndex) {
            m_out.lines.back().tableLastLine = true;
        }
    }

    /// Lay out every block in document order.
    void run() {
        for (const auto& block : m_doc.blocks) {
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
            case MdBlockKind::Table:
                emitTable(block);
                break;
            case MdBlockKind::Patch:
                emitPatch(block);
                break;
            }
        }
        m_out.width = m_width;
        m_out.height = m_yPos;
    }
};

} // namespace

auto fbide::layoutMarkdown(
    const MdDoc& doc,
    const int width,
    const TextMeasurer& measurer,
    const MarkdownPalette& palette,
    const CodeFenceHighlighter& highlightFence,
    const ImageResolver& resolveImage
) -> LaidOutDoc {
    Engine engine {
        doc,
        width,
        measurer,
        palette,
        highlightFence,
        resolveImage
    };
    engine.run();
    return std::move(engine.m_out);
}
