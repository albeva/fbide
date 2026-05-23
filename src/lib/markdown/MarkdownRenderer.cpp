//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownRenderer.hpp"
using namespace fbide;

namespace {
/// Vertical leading added on top of the measured "Ag" height — matches
/// what the chat view computes for the wheel-scroll quantum so that
/// per-style line height stays consistent across consumers.
constexpr int kLineLeading = 4;
} // namespace

DcMeasurer::DcMeasurer(
    wxDC& dcRef,
    wxFont body,
    wxFont mono,
    wxFont themed,
    std::vector<MeasurementEntry>& cache
)
: m_dc(dcRef)
, m_body(std::move(body))
, m_mono(std::move(mono))
, m_themed(std::move(themed))
, m_cache(cache) {}

auto DcMeasurer::width(const wxString& text, const TextStyle& style) const -> int {
    if (text.empty()) {
        return 0;
    }
    MeasurementEntry& entry = lookup(style);
    // Hot path — the wrap loop measures a space between every word/word
    // pair, always with the same style as its neighbours.
    if (text == " ") {
        if (entry.spaceWidth < 0) {
            entry.spaceWidth = measure(" ", entry.font);
        }
        return entry.spaceWidth;
    }
    return measure(text, entry.font);
}

auto DcMeasurer::lineHeight(const TextStyle& style) const -> int {
    MeasurementEntry& entry = lookup(style);
    if (entry.lineHeight < 0) {
        wxCoord textWidth = 0;
        wxCoord textHeight = 0;
        m_dc.GetTextExtent("Ag", &textWidth, &textHeight, nullptr, nullptr, &entry.font);
        entry.lineHeight = textHeight + kLineLeading;
    }
    return entry.lineHeight;
}

auto DcMeasurer::lookup(const TextStyle& style) const -> MeasurementEntry& {
    for (auto& entry : m_cache) {
        if (entry.style == style) {
            return entry;
        }
    }
    m_cache.push_back({ .style = style,
        .font = fontFor(style, m_body, m_mono, m_themed),
        .lineHeight = -1,
        .spaceWidth = -1 });
    return m_cache.back();
}

auto DcMeasurer::measure(const wxString& text, const wxFont& font) const -> int {
    wxCoord textWidth = 0;
    wxCoord textHeight = 0;
    m_dc.GetTextExtent(text, &textWidth, &textHeight, nullptr, nullptr, &font);
    return textWidth;
}

auto fbide::fontFor(
    const TextStyle& style,
    const wxFont& body,
    const wxFont& mono,
    const wxFont& themed
) -> wxFont {
    wxFont font = body;
    if (style.monospace) {
        font = style.themed ? themed : mono;
    }
    if (style.sizeDelta != 0) {
        font.SetPointSize(std::max(4, font.GetPointSize() + style.sizeDelta));
    }
    font.SetWeight(style.bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    font.SetStyle(style.italic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
    font.SetUnderlined(style.underline);
    font.SetStrikethrough(style.strikethrough);
    return font;
}

void fbide::paintLineBackground(
    wxGCDC& gc,
    const PaintLine& line,
    const int contentLeft,
    const int lineTop,
    const int contentWidth,
    const MarkdownPalette& palette
) {
    if (line.kind == LineKind::Code) {
        gc.SetBrush(wxBrush(palette.codeBg));
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.DrawRectangle(contentLeft, lineTop, contentWidth, line.height);
        return;
    }
    if (line.kind == LineKind::PatchSearch || line.kind == LineKind::PatchReplace) {
        // Two-tone background for a SEARCH/REPLACE proposal — same
        // padding pattern as a code block, just tinted red / green.
        const wxColour& fill = line.kind == LineKind::PatchSearch
                                 ? palette.patchSearchBg
                                 : palette.patchReplaceBg;
        gc.SetBrush(wxBrush(fill));
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.DrawRectangle(contentLeft, lineTop, contentWidth, line.height);
        return;
    }
    if (line.kind == LineKind::Rule) {
        gc.SetPen(wxPen(palette.rule));
        const int ruleY = lineTop + (line.height / 2);
        gc.DrawLine(contentLeft, ruleY, contentLeft + contentWidth, ruleY);
        return;
    }
    if ((line.kind == LineKind::TableHeader || line.kind == LineKind::TableBody)
        && !line.tableColumns.empty()) {
        // Header rows get a subtle background tint; the fill is clipped
        // to the table's actual column range so it doesn't leak out
        // into the rest of the surface. Borders / dividers go on top.
        const int leftEdge = contentLeft + line.tableColumns.front().x;
        const int rightEdge = contentLeft + line.tableColumns.back().x
                            + line.tableColumns.back().width;
        const int tableWidth = rightEdge - leftEdge;
        if (line.kind == LineKind::TableHeader) {
            gc.SetBrush(wxBrush(palette.tableHeaderBg));
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.DrawRectangle(leftEdge, lineTop, tableWidth, line.height);
        }
        gc.SetPen(wxPen(palette.rule));
        for (std::size_t col = 1; col < line.tableColumns.size(); col++) {
            const int colX = contentLeft + line.tableColumns.at(col).x;
            gc.DrawLine(colX, lineTop, colX, lineTop + line.height);
        }
        gc.DrawLine(leftEdge, lineTop, leftEdge, lineTop + line.height);
        gc.DrawLine(rightEdge, lineTop, rightEdge, lineTop + line.height);
        if (line.tableRowStart) {
            gc.DrawLine(leftEdge, lineTop, rightEdge, lineTop);
        }
        if (line.tableLastLine) {
            const int bottomY = lineTop + line.height;
            gc.DrawLine(leftEdge, bottomY, rightEdge, bottomY);
        }
        return;
    }
    if (line.kind == LineKind::Image && line.image.bitmap.IsOk()) {
        // Draw through the underlying wxGraphicsContext — `wxGCDC::DrawBitmap`
        // has no scaled-target overload, but the graphics context does.
        gc.GetGraphicsContext()->DrawBitmap(
            line.image.bitmap,
            contentLeft + line.image.x,
            lineTop,
            line.image.drawWidth,
            line.image.drawHeight
        );
        // The line's lone PaintRun is an empty hit-test region for the
        // click handler — `paintLineText` skips empty runs, so the
        // image line doesn't pick up any text drawing.
    }
}

namespace {

/// Greatest `n` such that `measurer.width(text.Mid(0, n), style) <= targetWidth`.
/// Binary-searches the prefix lengths; for short runs the cost is
/// negligible and only paid on the few runs a click hits.
auto charIndexForX(const wxString& text, const TextStyle& style, const int targetWidth, const TextMeasurer& measurer) -> std::size_t {
    if (text.empty() || targetWidth <= 0) {
        return 0;
    }
    const std::size_t length = text.length();
    std::size_t low = 0;
    std::size_t high = length;
    while (low < high) {
        const std::size_t mid = low + ((high - low + 1) / 2);
        if (measurer.width(text.Mid(0, mid), style) <= targetWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    return low;
}

} // namespace

auto fbide::hitTestLine(
    const PaintLine& line,
    const int xInContent,
    const TextMeasurer& measurer
) -> std::pair<std::size_t, std::size_t> {
    if (line.runs.empty()) {
        return { 0, 0 };
    }
    for (std::size_t runIdx = 0; runIdx < line.runs.size(); runIdx++) {
        const auto& run = line.runs.at(runIdx);
        if (run.text.empty()) {
            continue;
        }
        const int runRight = run.x + run.width;
        if (xInContent < runRight) {
            // Click is within (or to the left of) this run.
            const int xInRun = std::max(0, xInContent - run.x);
            return { runIdx, charIndexForX(run.text, run.style, xInRun, measurer) };
        }
    }
    // Click is past every run — land at the end of the last text-bearing run.
    for (std::size_t runIdx = line.runs.size(); runIdx-- > 0;) {
        const auto& run = line.runs.at(runIdx);
        if (!run.text.empty()) {
            return { runIdx, run.text.length() };
        }
    }
    return { 0, 0 };
}

namespace {

/// X position (relative to the line's left edge) of the caret described
/// by `(runIndex, charInRun)` within `line`. Snaps to the right edge of
/// the last text-bearing run when the position lands past the end of
/// the visible runs; returns 0 when the line has no text at all.
auto caretXInLine(const PaintLine& line, const std::size_t runIndex, const std::size_t charInRun, const TextMeasurer& measurer) -> int {
    if (runIndex < line.runs.size()) {
        const auto& run = line.runs.at(runIndex);
        const std::size_t clipped = std::min(charInRun, run.text.length());
        return run.x + measurer.width(run.text.Mid(0, clipped), run.style);
    }
    for (auto it = line.runs.rbegin(); it != line.runs.rend(); ++it) {
        if (!it->text.empty()) {
            return it->x + it->width;
        }
    }
    return 0;
}

} // namespace

void fbide::paintSelectionHighlight(
    wxGCDC& gc,
    const PaintLine& line,
    const std::size_t lineIndex,
    const int contentLeft,
    const int lineTop,
    const int contentWidth,
    const Selection& selection,
    const wxColour& highlightColour,
    const TextMeasurer& measurer
) {
    if (selection.empty()) {
        return;
    }
    const auto [startPos, endPos] = selection.range();
    // Skip lines entirely outside the selection range.
    if (lineIndex < startPos.lineIndex || lineIndex > endPos.lineIndex) {
        return;
    }
    const bool isStartLine = (lineIndex == startPos.lineIndex);
    const bool isEndLine = (lineIndex == endPos.lineIndex);

    // First-line bands start at the selection's caret; later lines fill
    // from the left edge of the content rect. End-line bands stop at
    // the selection's caret; earlier lines extend to the right edge so
    // the wrap-to-next-line behaviour reads visually.
    const int startX = isStartLine
                         ? caretXInLine(line, startPos.runIndex, startPos.charInRun, measurer)
                         : 0;
    const int endX = isEndLine
                       ? caretXInLine(line, endPos.runIndex, endPos.charInRun, measurer)
                       : contentWidth;
    if (startX >= endX) {
        return;
    }

    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(highlightColour));
    gc.DrawRectangle(contentLeft + startX, lineTop, endX - startX, line.height);
}

auto fbide::extractSelectedText(
    const LaidOutDoc& doc,
    const Selection& selection
) -> wxString {
    if (selection.empty()) {
        return {};
    }
    const auto [startPos, endPos] = selection.range();
    wxString out;
    for (std::size_t li = startPos.lineIndex; li <= endPos.lineIndex && li < doc.lines.size(); li++) {
        const auto& line = doc.lines.at(li);
        if (li > startPos.lineIndex) {
            out += '\n';
        }
        for (std::size_t runIdx = 0; runIdx < line.runs.size(); runIdx++) {
            const auto& run = line.runs.at(runIdx);
            if (run.text.empty()) {
                continue;
            }
            const bool atStart = (li == startPos.lineIndex);
            const bool atEnd = (li == endPos.lineIndex);
            if (atStart && runIdx < startPos.runIndex) {
                continue;
            }
            if (atEnd && runIdx > endPos.runIndex) {
                break;
            }
            const std::size_t selFrom = (atStart && runIdx == startPos.runIndex) ? startPos.charInRun : 0;
            const std::size_t selTo = (atEnd && runIdx == endPos.runIndex) ? endPos.charInRun : run.text.length();
            if (selFrom >= selTo) {
                continue;
            }
            out += run.text.Mid(selFrom, selTo - selFrom);
        }
    }
    return out;
}

void fbide::paintLineText(
    wxGCDC& gc,
    const PaintLine& line,
    const int contentLeft,
    const int lineTop,
    const wxFont& bodyFont,
    const wxFont& monoFont,
    const wxFont& themedFont,
    PaintRunState& state
) {
    // `selectRunFont` mutates `state` — when the style differs from the
    // cached one, it sets the DC font and refreshes `currentAscent` so
    // the cache spans both the ascent pass and the draw pass (and every
    // subsequent line that shares the style).
    const auto selectRunFont = [&](const PaintRun& run) -> wxCoord {
        if (!state.styleSet || run.style != state.currentStyle) {
            gc.SetFont(fontFor(run.style, bodyFont, monoFont, themedFont));
            state.currentStyle = run.style;
            state.currentAscent = gc.GetFontMetrics().ascent;
            state.styleSet = true;
        }
        return state.currentAscent;
    };

    wxCoord maxAscent = 0;
    for (const auto& run : line.runs) {
        if (run.text.empty()) {
            continue;
        }
        maxAscent = std::max(maxAscent, selectRunFont(run));
    }
    const wxCoord baseline = lineTop + 2 + maxAscent;

    for (const auto& run : line.runs) {
        if (run.text.empty()) {
            continue;
        }
        const wxCoord ascent = selectRunFont(run);
        if (!state.colourSet || run.colour != state.currentColour) {
            gc.SetTextForeground(run.colour);
            state.currentColour = run.colour;
            state.colourSet = true;
        }
        gc.DrawText(run.text, contentLeft + run.x, baseline - ascent);
    }
}
