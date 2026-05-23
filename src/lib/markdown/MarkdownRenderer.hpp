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
