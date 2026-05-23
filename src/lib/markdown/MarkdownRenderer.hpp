//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/dcgraph.h>
#include "markdown/MarkdownLayout.hpp"

namespace fbide {

/**
 * Stateless paint primitives for a `LaidOutDoc`.
 *
 * The renderer doesn't own any state of its own. Callers feed it a
 * `wxGCDC`, one laid-out line at a time, and the renderer draws the
 * line's background (code / patch / table / rule / image) or its text
 * runs (baseline-aligned, two-pass). A `PaintRunState` passed to
 * `paintLineText` carries the DC-cache across calls so adjacent
 * same-style runs and lines avoid redundant `SetFont` /
 * `GetFontMetrics` / `SetTextForeground` traffic.
 *
 * Both `AiChatView` (in its single-pass bubble paint) and the
 * standalone `MarkdownView` widget use these to render. Anything
 * cross-document — bubble decoration, action-bar overlay, applied-patch
 * veil — stays on the caller side.
 */

/// Resolve a concrete font for `style` from the three base fonts:
/// - `body`   → prose, headings, anything non-monospace.
/// - `mono`   → markdown inline `code` and non-themed fenced blocks.
/// - `themed` → blocks tagged with the host's theme font (the chat view
///              uses this for FreeBASIC fences); sized to match `body`.
[[nodiscard]] auto fontFor(
    const TextStyle& style,
    const wxFont& body,
    const wxFont& mono,
    const wxFont& themed
) -> wxFont;

/// One entry in the persistent text-measurement cache. Hosts hold a
/// `std::vector<MeasurementEntry>` and inject it into a `DcMeasurer`
/// so font lookups and per-style metrics survive across relayouts
/// (e.g. across streaming-tick rebuilds).
struct MeasurementEntry {
    TextStyle style {};
    wxFont font {};
    int lineHeight = -1; ///< Lazy-cached; -1 until measured.
    int spaceWidth = -1; ///< Lazy-cached width of a single space.
};

/// `TextMeasurer` backed by a wxDC + a host-owned cache. Same wxFonts
/// (body / mono / themed) are passed to `fontFor` to resolve each
/// `TextStyle` lazily; per-style results are memoised in `cache` so a
/// relayout sees only a handful of `GetTextExtent` calls. Both
/// `MarkdownView` and `AiChatView` construct one per paint / relayout
/// and pass it around.
class DcMeasurer final : public TextMeasurer {
public:
    DcMeasurer(
        wxDC& dcRef,
        wxFont body,
        wxFont mono,
        wxFont themed,
        std::vector<MeasurementEntry>& cache
    );

    [[nodiscard]] auto width(const wxString& text, const TextStyle& style) const -> int override;
    [[nodiscard]] auto lineHeight(const TextStyle& style) const -> int override;

private:
    [[nodiscard]] auto lookup(const TextStyle& style) const -> MeasurementEntry&;
    [[nodiscard]] auto measure(const wxString& text, const wxFont& font) const -> int;

    wxDC& m_dc;
    wxFont m_body;
    wxFont m_mono;
    wxFont m_themed;
    std::vector<MeasurementEntry>& m_cache;
};

/// Position inside a laid-out document — line index, run index within
/// that line, and a character index within that run. `charInRun == 0`
/// is "before the first character"; `charInRun == run.text.length()`
/// is "after the last character". Comparable so `Selection` can
/// normalise an unordered anchor/caret pair into a `[start, end)` range.
struct SelectionPosition {
    std::size_t lineIndex = 0;
    std::size_t runIndex = 0;
    std::size_t charInRun = 0;

    friend auto operator<=>(const SelectionPosition&, const SelectionPosition&) = default;
};

/// Text selection inside one laid-out document — anchor stays put as
/// the user drags; caret follows the pointer. `empty()` means no
/// highlight should be drawn. `range()` returns the pair normalised so
/// the first element is always `<=` the second, regardless of drag
/// direction.
struct Selection {
    SelectionPosition anchor;
    SelectionPosition caret;

    [[nodiscard]] auto empty() const -> bool { return anchor == caret; }
    [[nodiscard]] auto range() const -> std::pair<SelectionPosition, SelectionPosition> {
        if (anchor <= caret) {
            return { anchor, caret };
        }
        return { caret, anchor };
    }

    void clear() {
        anchor = caret = SelectionPosition {};
    }
};

/// Hit-test a click on a laid-out line: given the click's x-offset
/// inside the content rect (i.e. relative to `contentLeft`), return the
/// `{runIndex, charInRun}` that the click corresponds to. Clicks left
/// of all runs return `{0, 0}`; clicks right of all runs return the
/// end of the last run. The measurer is used to compute per-character
/// prefix widths inside a run.
[[nodiscard]] auto hitTestLine(
    const PaintLine& line,
    int xInContent,
    const TextMeasurer& measurer
) -> std::pair<std::size_t, std::size_t>;

/// Paint the selection highlight band for one line — a single filled
/// rect from the selection's left edge on this line to its right edge.
/// On the line that contains the selection start, the rect starts at
/// the start position and extends to `contentWidth` (showing the
/// selection wraps to the next line); on the line that contains the
/// end, it goes from `contentLeft` to the end position; lines fully
/// inside the range get a band the full content width. A single-line
/// selection collapses both edges onto the same line. Called BEFORE
/// `paintLineText` so the text sits on top.
///
/// `nextLineY` is the next laid line's `y` in document coords, or `-1`
/// when this is the last line. When the selection continues past this
/// line, the band's height is stretched to the next line's top so the
/// inter-block gap (the `kBlockGap` space layout leaves between
/// headings and paragraphs, etc.) is also painted.
void paintSelectionHighlight(
    wxGCDC& gc,
    const PaintLine& line,
    std::size_t lineIndex,
    int contentLeft,
    int lineTop,
    int contentWidth,
    int nextLineY,
    const Selection& selection,
    const wxColour& highlightColour,
    const TextMeasurer& measurer
);

/// Extract the rendered text of `selection` from `doc` as a single
/// `wxString`. Lines are joined with `\n`. The result is plain text —
/// no markdown markers are reconstructed.
[[nodiscard]] auto extractSelectedText(
    const LaidOutDoc& doc,
    const Selection& selection
) -> wxString;

/// Total non-space character offset of `position` from the start of
/// `doc`. Sums every preceding run's `text.length()` (spaces are
/// layout artefacts, not run characters) plus `position.charInRun`.
/// Stable across re-wrapping at a different width — the per-line
/// distribution of runs changes, but the total non-space character
/// count does not.
[[nodiscard]] auto selectionToOffset(const LaidOutDoc& doc, SelectionPosition position) -> std::size_t;

/// Inverse of `selectionToOffset` — locate the `(line, run, char)` at
/// `offset` non-space characters into `doc`. Out-of-range offsets
/// clamp to the last position. Pair with `selectionToOffset` to
/// preserve a `Selection` across a re-wrap at a new width.
[[nodiscard]] auto selectionFromOffset(const LaidOutDoc& doc, std::size_t offset) -> SelectionPosition;

/// DC-state cache carried across `paintLineText` calls. Adjacent runs
/// in the same paragraph almost always share style / colour; carrying
/// this across the loop turns `SetFont` from per-run to per-style.
struct PaintRunState {
    TextStyle currentStyle {};
    wxCoord currentAscent = 0;
    wxColour currentColour;
    bool styleSet = false;
    bool colourSet = false;
};

/// Paint a single laid-out line's background (code/patch tint strip,
/// horizontal rule, table fill + borders, or image bitmap). Text runs
/// are drawn separately by `paintLineText`.
void paintLineBackground(
    wxGCDC& gc,
    const PaintLine& line,
    int contentLeft,
    int lineTop,
    int contentWidth,
    const MarkdownPalette& palette
);

/// Two-pass baseline-aligned text draw for one laid-out line.
/// Pass 1 finds the max ascent across runs; pass 2 draws each run at
/// `baseline - runAscent`. `state` carries the cached font/colour
/// across calls so a single-style paragraph hits `SetFont` once.
void paintLineText(
    wxGCDC& gc,
    const PaintLine& line,
    int contentLeft,
    int lineTop,
    const wxFont& bodyFont,
    const wxFont& monoFont,
    const wxFont& themedFont,
    PaintRunState& state
);

} // namespace fbide
