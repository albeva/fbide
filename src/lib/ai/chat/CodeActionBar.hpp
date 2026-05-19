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

/// Window IDs of the `CodeActionBar` buttons. Each button's `wxEVT_BUTTON`
/// propagates to the host, which catches it by these IDs.
enum CodeActionId : int {
    ID_CodeCopy = wxID_HIGHEST + 1, ///< Copy the code to the clipboard.
    ID_CodeInsert,                  ///< Insert the code into the active editor.
    ID_CodeRun,                     ///< Compile and run the code.
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
    NO_COPY_AND_MOVE(CodeActionBar)

    /// Build the bar as a child of `parent`, loading its icons from `ctx`.
    CodeActionBar(wxWindow* parent, Context& ctx);

private:
    /// Add one flat icon button with window id `id` to `sizer`.
    void addButton(wxSizer* sizer, const wxBitmap& icon, int id, const wxString& tip);

    /// Fire `EVT_CODE_BAR_LEAVE` when the pointer truly leaves the bar.
    void onLeave(wxMouseEvent& event);
};

} // namespace fbide
