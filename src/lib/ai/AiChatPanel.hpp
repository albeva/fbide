//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

class wxHtmlWindow;

namespace fbide {
class Context;

/**
 * AI chat side panel: conversation view, message input box, send button.
 *
 * Docked on the right of the main frame as a hideable AUI pane, toggled
 * by the `viewAiChat` command (F7).
 *
 * **Owns:** its child controls (wx-parented).
 * **Owned by:** the main frame via `UIManager`.
 * **Threading:** UI thread only.
 *
 * Phase 1 stub — UI only. Model wiring lands in a later phase.
 */
class AiChatPanel final : public wxPanel {
public:
    NO_COPY_AND_MOVE(AiChatPanel)

    /// Build the panel and its controls as a child of `parent`.
    AiChatPanel(wxWindow* parent, Context& ctx);

private:
    /// Send button / Ctrl+Enter — echoes the input (stub until Phase 3).
    void onSend(wxCommandEvent& event);

    Context& m_ctx;                 ///< Application context.
    Unowned<wxHtmlWindow> m_output; ///< Conversation view (rendered HTML).
    Unowned<wxTextCtrl> m_input;    ///< Message input box.
    Unowned<wxButton> m_send;       ///< Send button.
};

} // namespace fbide
