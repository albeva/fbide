//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ai/chat/CodeHighlighter.hpp"
#include "markdown/Markdown.hpp"

namespace fbide {

/// Font selection for a run — flags and a size delta relative to the body
/// font. The measurer and the painter both resolve a concrete font from it.
struct TextStyle {
    int sizeDelta = 0;          ///< Point size relative to the body font.
    bool bold = false;          ///< Bold typeface.
    bool italic = false;        ///< Italic typeface.
    bool underline = false;     ///< Underlined (links).
    bool strikethrough = false; ///< Struck through.
    bool monospace = false;     ///< Monospace face (inline + block code).
    bool themed = false;        ///< FreeBASIC-highlighted code — use the
                                ///< editor theme's font face. Implies
                                ///< `monospace`; ignored when it isn't set.

    auto operator==(const TextStyle&) const -> bool = default;
};

/// Measures text for the layout engine. The production implementation wraps a
/// wxDC; tests inject a deterministic fake, so the wrap logic is unit testable
/// without a GUI.
class TextMeasurer {
public:
    TextMeasurer() = default;
    virtual ~TextMeasurer() = default;
    NO_COPY_AND_MOVE(TextMeasurer)

    /// Pixel width of `text` rendered in `style`.
    [[nodiscard]] virtual auto width(const wxString& text, const TextStyle& style) const -> int = 0;
    /// Pixel line height of `style`, leading included.
    [[nodiscard]] virtual auto lineHeight(const TextStyle& style) const -> int = 0;
};

/// Kind of a laid-out line — selects background and decoration when painting.
enum class LineKind : std::uint8_t {
    Prose,        ///< Wrapped prose / heading / list item.
    Code,         ///< A code-block line (or its padding) — painted on the code bg.
    Rule,         ///< A horizontal rule — no runs.
    TableHeader,  ///< Header row of a table — painted with a subtle tint.
    TableBody,    ///< Body row of a table — painted on the prose background.
    PatchSearch,  ///< Line in a SEARCH/REPLACE proposal's SEARCH half — red tint.
    PatchReplace, ///< Line in a SEARCH/REPLACE proposal's REPLACE half — green tint.
    Image,        ///< Embedded image — `image*` fields on PaintLine are set.
};

/// One column of a laid-out table row. Carries the column's geometry
/// and alignment so the painter can draw separators and the layout can
/// position cell text correctly.
struct TableColumn {
    int x = 0;     ///< Left edge, in document coordinates.
    int width = 0; ///< Pixel width allocated to this column.
};

/// One painted run: a span of same-styled text at a known offset.
struct PaintRun {
    wxString text;   ///< Run text.
    TextStyle style; ///< Font selection.
    wxColour colour; ///< Foreground colour.
    int x = 0;       ///< Left offset from the document's left edge.
    int width = 0;   ///< Measured pixel width.
    int linkId = -1; ///< Index into `LaidOutDoc::links`, or -1 if not a link.
};

/// One laid-out line, ready to paint.
struct PaintLine {
    LineKind kind = LineKind::Prose;
    int y = 0;          ///< Top offset within the document.
    int height = 0;     ///< Line height in pixels.
    int quoteDepth = 0; ///< Block-quote nesting — the painter draws that many bars.
    // NOLINTNEXTLINE(readability-redundant-member-init)
    std::vector<PaintRun> runs {};
    /// Table-only: per-column geometry. Empty for non-table lines. The
    /// painter draws vertical separators between columns and (for
    /// TableBody) a horizontal divider at the top of the row.
    // NOLINTNEXTLINE(readability-redundant-member-init)
    std::vector<TableColumn> tableColumns {};
    /// Table-only: true for the very LAST visual line of the table —
    /// the painter draws the bottom border on it.
    bool tableLastLine = false;
    /// Table-only: true for the first visual line of a table row. Cells
    /// that wrap to multiple lines produce extra PaintLines for the
    /// same row; the painter uses this flag to draw the row-divider
    /// only at the actual row start, not between wrapped lines.
    bool tableRowStart = false;
    /// Image-line only: bitmap, click target, alt label and the
    /// scaled draw rect. Packed in a struct so designated initializers
    /// elsewhere don't have to list every image field.
    struct ImageContent {
        wxBitmap bitmap;
        wxString url; ///< Source URL — also the click target.
        wxString alt; ///< Alt text — tooltip / accessibility.
        int drawWidth = 0;
        int drawHeight = 0;
        int x = 0; ///< Left offset of the image rect inside the bubble.
    };
    ImageContent image {};
};

/// A hyperlink target, referenced by `PaintRun::linkId`.
struct LinkTarget {
    wxString url;
};

/// A laid-out fenced code block — its rectangle within the document. The
/// view places a toolbar over it; when the user clicks an action, the
/// snippet body is resolved on demand from the source markdown via
/// `resolveCodeBlockText` so the laid-out doc doesn't carry a duplicate
/// copy of every snippet.
struct LaidCodeBlock {
    int y = 0;      ///< Top offset within the document (includes padding).
    int height = 0; ///< Total height including padding strips.
};

/// A laid-out SEARCH/REPLACE proposal — its region within the document
/// plus the raw search/replace strings, so the view can place an action
/// bar (Apply / Reject) over it and resolve the edit when applied.
struct LaidPatchBlock {
    wxString target;  ///< Optional target path from the SEARCH header.
    wxString search;  ///< Verbatim SEARCH text.
    wxString replace; ///< Verbatim REPLACE text.
    int y = 0;        ///< Top offset within the document (includes padding).
    int height = 0;   ///< Total height including padding strips.
};

/// Colours the layout and painter need that are not carried on code runs.
struct MarkdownPalette {
    wxColour text;           ///< Body prose colour.
    wxColour link;           ///< Hyperlink colour.
    wxColour codeBg;         ///< Code-block background (from editor theme).
    wxColour inlineCodeBg;   ///< Inline `code` background.
    wxColour rule;           ///< Horizontal-rule colour.
    wxColour tableHeaderBg;  ///< Table header-row tint. Derived from system
                             ///< colours (not the editor theme) so it stays
                             ///< distinct from `text` in both light and dark
                             ///< modes regardless of the editor theme.
    wxColour patchSearchBg;  ///< SEARCH half of a patch proposal — red tint.
    wxColour patchReplaceBg; ///< REPLACE half of a patch proposal — green tint.
    wxColour patchFg;        ///< Text colour for patch SEARCH/REPLACE lines.
};

/// A fully laid-out document — stacked, wrapped lines plus link targets.
struct LaidOutDoc {
    std::vector<PaintLine> lines;
    std::vector<LinkTarget> links;
    std::vector<LaidCodeBlock> codeBlocks;   ///< Fenced code-block regions.
    std::vector<LaidPatchBlock> patchBlocks; ///< SEARCH/REPLACE proposal regions.
    int width = 0;                           ///< Width the document was laid out for.
    int height = 0;                          ///< Total stacked height.
};

/// Highlights a fenced code block — `code` body, `lang` fence tag — into
/// coloured lines. Injected so the layout stays independent of the lexer.
using CodeFenceHighlighter
    = std::function<std::vector<CodeLine>(const wxString& code, const wxString& lang)>;

/// Image lookup result handed to the layout for an `MdInlineKind::Image`.
/// `Loading` and `Failed` cause the layout to emit a placeholder prose line;
/// `Ready` populates a dedicated `LineKind::Image` line.
struct ImageInfo {
    enum class State : std::uint8_t { Loading,
        Ready,
        Failed };
    State state = State::Failed;
    wxBitmap bitmap;
    int width = 0;
    int height = 0;
};

/// Resolve a markdown image URL to its current cache state. Called once per
/// image inline per layout pass; the caller (`AiChatView`) is expected to
/// relayout when the underlying cache transitions to Ready/Failed.
using ImageResolver = std::function<ImageInfo(const wxString& url)>;

/// Lay `doc` out into stacked, wrapped lines fitting `width` pixels. Code
/// blocks are highlighted through `highlightFence`; prose colours come from
/// `palette`; all measurement goes through `measurer`. Image inlines are
/// resolved via `resolveImage` — when omitted, every image is treated as
/// permanently `Failed` (placeholder text only).
[[nodiscard]] auto layoutMarkdown(
    const MdDoc& doc,
    int width,
    const TextMeasurer& measurer,
    const MarkdownPalette& palette,
    const CodeFenceHighlighter& highlightFence,
    const ImageResolver& resolveImage = {}
) -> LaidOutDoc;

} // namespace fbide
