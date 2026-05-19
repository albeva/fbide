//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Floating action toolbar shown over a code block in the chat view.
 *
 * A small real toolbar — flat icon buttons for copying the code, inserting
 * it into the editor and compiling + running it. One instance is reused:
 * `AiChatView` moves and shows it over whichever code block is hovered.
 *
 * **Owns:** its buttons (wx-parented).
 * **Owned by:** `AiChatView` (wx-parented).
 */
class CodeActionBar final : public wxPanel {
public:
    NO_COPY_AND_MOVE(CodeActionBar)

    /// Callback run when an action button is clicked.
    using Action = std::function<void()>;

    /// Build the bar with the three action icons and their handlers.
    CodeActionBar(
        wxWindow* parent,
        const wxBitmap& copyIcon,
        const wxBitmap& insertIcon,
        const wxBitmap& runIcon,
        Action onCopy,
        Action onInsert,
        Action onRun
    );

    /// Set the handler invoked when the pointer genuinely leaves the bar
    /// (moving onto a child button does not count). Lets the host decide
    /// whether the bar should stay visible.
    void setLeaveHandler(std::function<void()> handler) { m_onLeave = std::move(handler); }

private:
    void onLeave(wxMouseEvent& event);

    std::function<void()> m_onLeave; ///< Pointer-left-the-bar handler.
};

} // namespace fbide
