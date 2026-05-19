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

/// Which action a `CodeActionBar` button triggers. Carried as the integer
/// payload (`wxCommandEvent::GetInt`) of an `EVT_CODE_ACTION` event.
enum class CodeAction : int {
    Copy,   ///< Copy the code to the clipboard.
    Insert, ///< Insert the code into the active editor.
    Run,    ///< Compile and run the code.
};

/// Emitted by `CodeActionBar` when an action button is clicked; the event's
/// `GetInt()` is the `CodeAction`.
wxDECLARE_EVENT(EVT_CODE_ACTION, wxCommandEvent);

/// Emitted by `CodeActionBar` when the pointer genuinely leaves the bar.
wxDECLARE_EVENT(EVT_CODE_BAR_LEAVE, wxCommandEvent);

/**
 * Floating action toolbar shown over a code block in the chat view.
 *
 * A small real toolbar — flat icon buttons for copying the code, inserting
 * it into the editor and compiling + running it. It loads its own icons
 * from the application's `ArtiProvider` and emits `EVT_CODE_ACTION` /
 * `EVT_CODE_BAR_LEAVE`; the host listens for those.
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
    /// Add one flat icon button for `action` to `sizer`.
    void addButton(wxSizer* sizer, const wxBitmap& icon, CodeAction action, const wxString& tip);

    /// Fire an `EVT_CODE_ACTION` carrying `action`.
    void emitAction(CodeAction action);

    /// Fire `EVT_CODE_BAR_LEAVE` when the pointer truly leaves the bar.
    void onLeave(wxMouseEvent& event);
};

} // namespace fbide
