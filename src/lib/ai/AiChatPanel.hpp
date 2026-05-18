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
 * by the `viewAiChat` command (F7). Sends messages through `AiManager`
 * and renders the conversation as HTML (markdown replies converted via
 * maddy).
 *
 * **Owns:** its child controls (wx-parented).
 * **Owned by:** the main frame via `UIManager`.
 * **Threading:** UI thread only.
 */
class AiChatPanel final : public wxPanel {
public:
    NO_COPY_AND_MOVE(AiChatPanel)

    /// Build the panel and its controls as a child of `parent`.
    AiChatPanel(wxWindow* parent, Context& ctx);

private:
    /// Send button — dispatches the input box text through `AiManager`.
    void onSend(wxCommandEvent& event);

    /// Re-render the whole conversation (plus busy / error state) into
    /// the HTML view.
    void renderConversation();

    Context& m_ctx;                 ///< Application context.
    Unowned<wxHtmlWindow> m_output; ///< Conversation view (rendered HTML).
    Unowned<wxTextCtrl> m_input;    ///< Message input box.
    Unowned<wxButton> m_send;       ///< Send button.
    wxString m_lastError;           ///< Last request error, shown until the next send.
    bool m_busy = false;            ///< True while a request is in flight.
};

} // namespace fbide
