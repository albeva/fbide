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
class AiChatView;

/**
 * AI chat side panel: conversation view, message input box, send button,
 * and a context bar for attaching files.
 *
 * Docked on the right of the main frame as a hideable AUI pane, toggled
 * by the `viewAiChat` command (F7). Sends messages through `AiManager`
 * and renders the conversation in a custom-painted `AiChatView` (markdown
 * parsed and laid out natively). Replies stream in incrementally; a
 * throttle timer limits how often the view is rebuilt.
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

    /// Send `text` to the model as a chat message and render the reply.
    /// Used by the send button and by editor code actions. No-op when a
    /// request is already in flight or `text` is empty.
    void submitPrompt(const wxString& text);

private:
    /// Send button — dispatches the input box text through `AiManager`.
    void onSend(wxCommandEvent& event);

    /// Throttle tick — re-renders if streamed text arrived since the last.
    void onRenderTimer(wxTimerEvent& event);

    /// "Add file" — pick files and attach them as conversation context.
    void onAddFile(wxCommandEvent& event);

    /// "Remove" — drop the selected context file.
    void onRemoveFile(wxCommandEvent& event);

    /// Rebuild the conversation in the view from `AiManager`'s history,
    /// plus the streaming reply and any error.
    void renderConversation();

    /// Repopulate the context list box from `AiManager`'s context.
    void refreshContextList();

    Context& m_ctx;                   ///< Application context.
    Unowned<AiChatView> m_output;     ///< Conversation view (custom-painted).
    Unowned<wxTextCtrl> m_input;      ///< Message input box.
    Unowned<wxButton> m_send;         ///< Send button.
    Unowned<wxListBox> m_contextList; ///< Attached context files.
    Unowned<wxButton> m_addFile;      ///< "Add file" button.
    Unowned<wxButton> m_removeFile;   ///< "Remove" button.
    wxString m_streaming;             ///< Partial assistant reply while streaming.
    wxString m_lastError;             ///< Last request error, shown until the next send.
    wxTimer m_renderTimer;            ///< Throttles re-render while streaming.
    bool m_busy = false;              ///< True while a request is in flight.
    bool m_dirty = false;             ///< Streamed text arrived since the last render.
};

} // namespace fbide
