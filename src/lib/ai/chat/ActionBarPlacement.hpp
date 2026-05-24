//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/**
 * Tracks which code block the floating action bar is anchored to and
 * computes where to place the bar relative to that block.
 *
 * Doesn't own the bar widget — the chat view does, because the bar is
 * a wx child for the scrolling behaviour. This class just holds the
 * (messageIndex, blockIndex) anchor pair and the attached-vs-pinned
 * placement math so neither lives as scattered ints + open code in
 * AiChatView.
 */
class ActionBarPlacement final {
public:
    NO_COPY_AND_MOVE(ActionBarPlacement)

    /// Inset between the action bar and the code block's top-right
    /// corner (or, when pinned, the top of the visible area).
    static constexpr int kInset = 4;

    ActionBarPlacement() = default;

    /// True between `setAnchor` and the matching `clearAnchor`.
    [[nodiscard]] auto active() const -> bool { return m_messageIndex >= 0; }
    [[nodiscard]] auto messageIndex() const -> int { return m_messageIndex; }
    [[nodiscard]] auto blockIndex() const -> int { return m_blockIndex; }

    /// Anchor the placement to a specific (message, block) pair. Both
    /// indices are stored as-is; `messageIndex < 0` ends the anchor.
    void setAnchor(const int messageIndex, const int blockIndex) {
        m_messageIndex = messageIndex;
        m_blockIndex = blockIndex;
    }

    void clearAnchor() {
        m_messageIndex = -1;
        m_blockIndex = -1;
    }

    /// Bar position in scroll-surface client coordinates.
    struct Position {
        int x = 0;
        int y = 0;
    };

    /// Compute the bar's position next to a code block whose top-right
    /// corner is at `(codeRightDoc, codeTopDoc)` in document coords.
    ///
    /// While the block's top is inside the viewport the bar tracks the
    /// block and scrolls with the content (attached). Once the top has
    /// scrolled above the viewport, the bar pins to `viewTopClient`
    /// (typically the scrolled window's own y in its parent) so it
    /// stays visible.
    [[nodiscard]] static auto computePosition(
        int codeRightDoc,
        int codeTopDoc,
        int originY,
        int viewTopClient,
        wxSize barSize
    ) -> Position;

private:
    int m_messageIndex = -1;
    int m_blockIndex = -1;
};

} // namespace fbide::ai
