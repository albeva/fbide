//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
} // namespace fbide

namespace fbide::ai {

/// Window IDs of the `CodeActionBar` buttons. Each button's `wxEVT_BUTTON`
/// propagates to the host, which catches it by these IDs.
enum CodeActionId : int {
    ID_CodeCopy = wxID_HIGHEST + 1, ///< Copy the code to the clipboard.
    ID_CodeInsert,                  ///< Insert the code into the active editor.
    ID_CodeRun,                     ///< Compile and run the code.
    ID_PatchApply,                  ///< Apply a SEARCH/REPLACE proposal.
    ID_PatchReject,                 ///< Reject a SEARCH/REPLACE proposal.
    ID_BlockCollapse,               ///< Hide the block body, leaving just the summary strip.
    ID_BlockExpand,                 ///< Restore the full block body from a collapsed strip.
};

/// Emitted by `CodeActionBar` when the pointer genuinely leaves the bar.
wxDECLARE_EVENT(EVT_CODE_BAR_LEAVE, wxCommandEvent);

/**
 * Floating action toolbar shown over a code block in the chat view.
 *
 * A small real toolbar — flat icon buttons for copying the code, inserting
 * it into the editor and compiling + running it. It loads its own icons
 * from the application's `ArtiProvider`. The buttons carry the `CodeActionId`
 * IDs; their `wxEVT_BUTTON` events propagate to the host. The bar also emits
 * `EVT_CODE_BAR_LEAVE` when the pointer leaves it.
 *
 * **Owns:** its buttons (wx-parented).
 * **Owned by:** `AiChatView` (wx-parented).
 */
class CodeActionBar final : public wxPanel {
public:
    /// Per-button visibility bit. The host (typically `AiChatView`)
    /// composes a bitmask of buttons it wants visible and feeds it to
    /// `setButtons`; everything else hides. This decouples the bar
    /// from the host's "is the anchored block a code block? a patch?
    /// collapsed? a one-liner?" decision tree — the bar just shows
    /// what it's told.
    enum Button : std::uint8_t {
        Copy = 1 << 0,     ///< Copy code to clipboard.
        Insert = 1 << 1,   ///< Insert code into the active editor.
        Run = 1 << 2,      ///< Compile + run code.
        Apply = 1 << 3,    ///< Apply SEARCH/REPLACE patch.
        Reject = 1 << 4,   ///< Dismiss the patch proposal.
        Collapse = 1 << 5, ///< Hide the block body, leaving a summary strip.
        Expand = 1 << 6,   ///< Restore a collapsed block's body.
    };

    NO_COPY_AND_MOVE(CodeActionBar)

    /// Build the bar as a child of `parent`, loading its icons from `ctx`.
    CodeActionBar(wxWindow* parent, Context& ctx);

    /// Show the buttons whose `Button` bit is set in `buttons`; hide
    /// the rest. Re-runs the layout and resizes the bar to fit. No-op
    /// when `buttons` is already the active mask.
    void setButtons(std::uint8_t buttons);

    /// Current visibility mask — the host uses this to skip
    /// redundant reconfiguration during hover dispatch.
    [[nodiscard]] auto buttons() const -> std::uint8_t { return m_buttons; }

private:
    /// Add one flat icon button to the bar. The `Button` bit is stored
    /// as the button's `wxWindow::ClientData` so `setButtons` can
    /// mask-test it without a side table.
    void addButton(
        Button button,
        const wxBitmap& icon,
        int id,
        const wxString& tip
    );

    /// Fire `EVT_CODE_BAR_LEAVE` when the pointer truly leaves the bar.
    void onLeave(wxMouseEvent& event);

    /// Custom paint — fills the background and strokes a 1-px border at
    /// the client-rect edge. Owning the border paint keeps it on the
    /// blit-able pixel surface so it follows the parent's scroll
    /// cleanly (avoids the `wxBORDER_SIMPLE` ghost-line artefact).
    void onPaint(wxPaintEvent& event);

    std::uint8_t m_buttons = 0; ///< Currently-visible buttons (`Button` bitmask).

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide::ai
