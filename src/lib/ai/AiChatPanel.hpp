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
class ContextTagBar;
class Document;

/**
 * AI chat side panel: conversation view, a context tag strip, the message
 * input box, an attach (+) button and a send button.
 *
 * Docked on the right of the main frame as a hideable AUI pane, toggled
 * by the `viewAiChat` command (F7). Sends messages through `AiManager`
 * and renders the conversation in a custom-painted `AiChatView`. Replies
 * stream in incrementally; a throttle timer limits how often the view is
 * rebuilt. The input box grows with its content; the `+` button attaches
 * tabs / includes / files as conversation context, shown as removable
 * tags in `ContextTagBar`.
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

    /// Re-render the conversation after editor settings (theme / keywords)
    /// have changed — code blocks pick up the new colours and font.
    void refreshTheme();

private:
    /// Send button — dispatches the input box text through `AiManager`.
    void onSend(wxCommandEvent& event);

    /// Throttle tick — re-renders if streamed text arrived since the last.
    void onRenderTimer(wxTimerEvent& event);

    /// Input box changed — grow / shrink it to fit.
    void onInputText(wxCommandEvent& event);

    /// "+" button — pop the attach-context menu.
    void onAddContext(wxCommandEvent& event);

    /// `EVT_CONTEXT_TAGS_CHANGED` from the tag bar — re-lay the panel.
    void onTagsChanged(wxCommandEvent& event);

    /// Grow / shrink the input box to fit its content (line-count capped).
    void autoSizeInput();

    /// Attach `doc`'s current editor text as a context snapshot.
    void attachDocument(Document* doc);

    /// Re-render the whole conversation (plus the streaming reply and any
    /// error) into the chat view.
    void renderConversation();

    Context& m_ctx;                  ///< Application context.
    Unowned<AiChatView> m_output;    ///< Conversation view (custom-painted).
    Unowned<ContextTagBar> m_tagBar; ///< Attached-context tag strip.
    Unowned<wxTextCtrl> m_input;     ///< Message input box.
    Unowned<wxButton> m_addContext;  ///< "+" attach-context button.
    Unowned<wxButton> m_send;        ///< Send button.
    wxString m_streaming;            ///< Partial assistant reply while streaming.
    wxString m_lastError;            ///< Last request error, shown until the next send.
    wxTimer m_renderTimer;           ///< Throttles re-render while streaming.
    bool m_busy = false;             ///< True while a request is in flight.
    bool m_dirty = false;            ///< Streamed text arrived since the last render.
};

} // namespace fbide
