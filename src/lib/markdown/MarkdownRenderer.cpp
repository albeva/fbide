//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownRenderer.hpp"
using namespace fbide;

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
        for (std::size_t c = 1; c < line.tableColumns.size(); c++) {
            const int colX = contentLeft + line.tableColumns[c].x;
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
        const wxCoord ascent = selectRunFont(run);
        if (ascent > maxAscent) {
            maxAscent = ascent;
        }
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
