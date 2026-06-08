//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "markdown/CodeFence.hpp"
#include "markdown/Markdown.hpp"

namespace fbide::markdown {

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
    Prose,          ///< Wrapped prose / heading / list item.
    Code,           ///< A code-block line (or its padding) — painted on the code bg.
    Rule,           ///< A horizontal rule — no runs.
    TableHeader,    ///< Header row of a table — painted with a subtle tint.
    TableBody,      ///< Body row of a table — painted on the prose background.
    PatchSearch,    ///< Line in a SEARCH/REPLACE proposal's SEARCH half — red tint.
    PatchReplace,   ///< Line in a SEARCH/REPLACE proposal's REPLACE half — green tint.
    Image,          ///< Embedded image — `image*` fields on PaintLine are set.
    CollapsedBlock, ///< Sole line of a collapsed code / patch block — painter reads
                    ///< the summary strings from the containing `LaidScrollBlock`.
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
    /// scaled draw rect. Allocated lazily via `unique_ptr` so prose
    /// lines (the overwhelming majority) carry just one pointer rather
    /// than a full wxBitmap + 2 wxStrings + 5 ints of empty payload.
    struct ImageContent {
        wxBitmap bitmap;
        wxString url; ///< Source URL — also the click target.
        wxString alt; ///< Alt text — tooltip / accessibility.
        int drawWidth = 0;
        int drawHeight = 0;
        int x = 0; ///< Left offset of the image rect inside the bubble.
    };
    std::unique_ptr<ImageContent> image;
    /// Index into `LaidOutDoc::scrollBlocks` when this line belongs to
    /// a fenced code block or a SEARCH/REPLACE proposal, or -1 for
    /// anything else. The line's `kind` distinguishes Code vs
    /// PatchSearch vs PatchReplace for paint purposes. Lets the
    /// painter / hit-tester resolve the containing block in O(1)
    /// without scanning the block list per line.
    int blockIndex = -1;
};

/// A hyperlink target, referenced by `PaintRun::linkId`.
struct LinkTarget {
    wxString url;
};

/// A laid-out block that owns a content rectangle — fenced code block or
/// SEARCH/REPLACE proposal. Both have identical geometry; the patch fields
/// (`patchTarget` / `patchSearch` / `patchReplace`) carry the verbatim
/// strings the view needs to resolve an Apply, and stay empty for Code
/// blocks. The view places its toolbar / Apply-Reject bar over the
/// block's rect; for code snippets the body is re-resolved on demand
/// from the source markdown via `resolveCodeBlockText` so the laid doc
/// doesn't keep a duplicate copy.
struct LaidScrollBlock {
    enum class Kind : std::uint8_t { Code,
        Patch };
    Kind kind = Kind::Code;
    int y = 0;              ///< Top offset within the document (includes padding).
    int height = 0;         ///< Total height including padding strips.
    int contentLeft = 0;    ///< Left edge of the text area inside the block (document coords).
    int contentWidth = 0;   ///< Visible width of the text area — what's not scrolled is clipped to this.
    int naturalWidth = 0;   ///< Right edge of the widest run minus `contentLeft`. Equals
                            ///< `contentWidth` when the block was laid out with wrapping
                            ///< on; larger when wrapping is off and lines overflow.
    bool wrapped = true;    ///< Layout-mode flag — false when the block was laid out
                            ///< with horizontal scroll instead of soft wrap.
    bool collapsed = false; ///< True when the block was laid out as a single summary
                            ///< strip; the painter reads `summary*` and ignores the
                            ///< usual content lines. Default false so existing call
                            ///< sites (MarkdownView, tests) keep the expanded shape.
    bool themed = false;    ///< Mirrors what `emitCode` used for `TextStyle::themed` —
                            ///< true for FreeBASIC fences so the editor-theme
                            ///< monospace font is used. Lets the painter pick the
                            ///< same face when drawing a collapsed strip without
                            ///< re-deriving the predicate from `codeLang`.
    /// Responsive summary text for collapsed blocks. The painter picks the
    /// widest version that fits the strip's content width — keeping the
    /// fallback chain (narrow → mid → wide) inside the laid doc so the
    /// painter doesn't have to re-derive line counts / language at paint.
    /// All three stay empty when `collapsed` is false.
    /// Components the painter assembles into the responsive summary
    /// strip. Storing them structured (instead of three precomputed
    /// strings) lets the painter apply git-style colouring to the
    /// `+added` / `-removed` tokens and rebuild the language label
    /// from a host-supplied display name lookup.
    struct CollapsedSummary {
        wxString languageDisplay; ///< Resolved language label — e.g. `FreeBASIC`. Empty when unknown.
        int added = 0;            ///< Code: total source lines. Patch: REPLACE-side line count.
        int removed = 0;          ///< Code: 0 (no diff baseline). Patch: SEARCH-side line count.
    };
    CollapsedSummary summary; ///< Populated only when `collapsed` is true.
    wxString codeText;        ///< Code only: verbatim fenced text — kept so callers
                              ///< can compute stable identity hashes across
                              ///< re-layouts without re-parsing the markdown source.
    wxString codeLang;        ///< Code only: lower-cased fence tag (empty when none).
    wxString patchTarget;     ///< Patch only: optional target path from the SEARCH header.
    wxString patchSearch;     ///< Patch only: verbatim SEARCH text.
    wxString patchReplace;    ///< Patch only: verbatim REPLACE text.
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
    wxColour added;          ///< Foreground for git-style `+N` tokens (green).
    wxColour removed;        ///< Foreground for git-style `-N` tokens (red).
};

/// A fully laid-out document — stacked, wrapped lines plus link targets.
struct LaidOutDoc {
    std::vector<PaintLine> lines;
    std::vector<LinkTarget> links;
    /// Fenced code blocks and SEARCH/REPLACE proposals in document order —
    /// each carries a `kind` discriminator. `PaintLine::blockIndex` points
    /// here for lines that belong to one of these blocks.
    std::vector<LaidScrollBlock> scrollBlocks;
    int width = 0;  ///< Width the document was laid out for.
    int height = 0; ///< Total stacked height.
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

/// Resolve a fence language tag (e.g. `freebasic`, `json`) to a
/// user-facing display name (e.g. `FreeBASIC`, `JSON`). Optional —
/// when omitted, the layout drops the wide-tier summary.
///
/// `kind` tells the resolver whether the call is for a fenced code
/// block (`Code`) or a patch (`Patch`), so a host can supply
/// different fallbacks for the two. For patches the `lang` argument
/// is the patch target (filename / path) when one was parsed out of
/// the SEARCH header; when empty, a host with a pinned edit target
/// can fall back to the target's filetype so collapsed strips still
/// read as `FreeBASIC +10 -30` rather than the bare counts.
using LanguageDisplayResolver = std::function<wxString(
    LaidScrollBlock::Kind kind,
    const wxString& lang
)>;

/// Decide whether a fenced code block / patch proposal should be laid out
/// collapsed (a single summary strip) instead of with its full body.
///
/// Called once per block during the layout pass. `kind` distinguishes the
/// two cases; for `Code` blocks `lang` is the fence tag and `contentA`
/// is the raw code body (`contentB` is empty). For `Patch` blocks
/// `lang` is the optional `patchTarget`, `contentA` is the SEARCH text
/// and `contentB` is the REPLACE text. Returning `true` makes the layout
/// emit a one-line `CollapsedBlock` row instead of the full block; the
/// summary strings are computed by the layout itself from the same
/// content.
///
/// Empty default = no collapsing — keeps non-chat callers (the markdown
/// view, tests) on the historical fully-expanded shape with zero
/// per-call boilerplate.
using BlockCollapsedQuery = std::function<bool(
    LaidScrollBlock::Kind kind,
    const wxString& lang,
    const wxString& contentA,
    const wxString& contentB
)>;

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
    const ImageResolver& resolveImage = {},
    bool wrapCodeBlocks = true,
    const BlockCollapsedQuery& isCollapsed = {},
    const LanguageDisplayResolver& resolveLanguageDisplay = {}
) -> LaidOutDoc;

} // namespace fbide::markdown
