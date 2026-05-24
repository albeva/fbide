//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/coup-fbide
//
#pragma once
#include "pch.hpp"
#include "markdown/MarkdownRenderer.hpp"

namespace fbide::markdown {
struct LaidOutDoc;
} // namespace fbide::markdown

namespace fbide::ai {

/**
 * Selection state for the AI chat view.
 *
 * Tracks which bubble owns the highlight, the anchor / caret positions
 * inside that bubble's laid document, and whether a drag-select is
 * currently in progress. Owns no widgets and triggers no repaints —
 * the chat view drives this through mouse / keyboard events and
 * repaints when it needs to.
 */
class ChatSelection final {
public:
    NO_COPY_AND_MOVE(ChatSelection)

    ChatSelection() = default;

    /// True when a bubble owns the selection (`begin` / `selectWord` /
    /// `selectAll` was called and `clear` has not since).
    [[nodiscard]] auto active() const -> bool { return m_messageIndex >= 0; }

    /// Index into the chat view's bubble list, or -1 when no bubble
    /// owns the selection.
    [[nodiscard]] auto messageIndex() const -> int { return m_messageIndex; }

    /// True when anchor == caret — nothing is highlighted yet, even if
    /// `active()`. Useful to suppress empty-range paints.
    [[nodiscard]] auto empty() const -> bool { return m_selection.empty(); }

    /// True between `begin` (or `selectWord` / `selectAll`) and the
    /// matching `setDragging(false)`. The view uses this to keep the
    /// caret following the pointer instead of dispatching hover.
    [[nodiscard]] auto dragging() const -> bool { return m_dragging; }

    /// Underlying anchor + caret pair — for paintMessage's highlight
    /// pass and extractSelectedText.
    [[nodiscard]] auto data() const -> const markdown::Selection& { return m_selection; }

    /// Start a new selection at `position` in bubble `messageIndex`.
    /// Implicitly enters drag mode — the caller calls `setDragging(false)`
    /// when the pointer is released.
    void begin(int messageIndex, markdown::SelectionPosition position);

    /// Extend the existing caret to `position` (shift-click semantics).
    /// No-op when the selection isn't active.
    void extendCaret(markdown::SelectionPosition position);

    /// Update the caret during a drag-select. No-op when not dragging.
    void dragCaret(markdown::SelectionPosition position);

    /// Drop the selection. After this `active()` and `dragging()` are
    /// both false.
    void clear();

    /// Set / clear the dragging flag explicitly. Called by the view's
    /// mouse-up handler to finalise the drag.
    void setDragging(const bool on) { m_dragging = on; }

    /// Select the word at `position` in bubble `messageIndex`, given
    /// the bubble's laid document for run lookup.
    void selectWord(int messageIndex, markdown::SelectionPosition position, const markdown::LaidOutDoc& laid);

    /// Select everything in bubble `messageIndex` given its laid doc.
    void selectAll(int messageIndex, const markdown::LaidOutDoc& laid);

    /// Anchor / caret saved as flat character offsets — stable across a
    /// relayout where line / run boundaries shift.
    struct StableOffsets {
        std::size_t anchor = 0;
        std::size_t caret = 0;
    };

    /// Capture anchor + caret as flat offsets inside the currently-
    /// selected bubble's laid doc. Caller passes the pre-relayout doc.
    [[nodiscard]] auto captureOffsets(const markdown::LaidOutDoc& laid) const -> StableOffsets;

    /// Re-derive anchor + caret from `offsets` using the post-relayout
    /// `laid`. Used after a width change reflows the doc.
    void restoreFromOffsets(const markdown::LaidOutDoc& laid, StableOffsets offsets);

private:
    int m_messageIndex = -1;
    markdown::Selection m_selection;
    bool m_dragging = false;
};

} // namespace fbide::ai
