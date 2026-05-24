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
    /// Which set of buttons the bar currently presents. Driven by the
    /// host based on what is under the cursor — a fenced code block
    /// (CodeSample) or a SEARCH/REPLACE proposal (PatchProposal).
    enum class Mode : std::uint8_t {
        CodeSample,    ///< Copy / Insert / Run.
        PatchProposal, ///< Apply / Reject.
    };

    NO_COPY_AND_MOVE(CodeActionBar)

    /// Build the bar as a child of `parent`, loading its icons from `ctx`.
    CodeActionBar(wxWindow* parent, Context& ctx);

    /// Switch which button group is visible. Hides the inactive group,
    /// re-runs the layout, and resizes the bar to fit. No-op when the
    /// mode is already current.
    void setMode(Mode mode);

    /// Current mode — useful for the host's hover dispatch to skip
    /// redundant reconfiguration.
    [[nodiscard]] auto mode() const -> Mode { return m_mode; }

private:
    /// Add one flat icon button with window id `id` to `sizer`, and
    /// remember it under `group` so `setMode` can show / hide the set.
    void addButton(
        wxSizer* sizer,
        Mode* mode,
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

    Mode m_mode = Mode::CodeSample;        ///< Current visible set.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide::ai
