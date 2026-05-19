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

/// One conversation message handed to the chat view.
struct ChatViewMessage {
    bool fromUser = false; ///< true = user prompt, false = assistant reply.
    wxString markdown;     ///< Message body — markdown source.
};

/**
 * Container panel for the AI conversation.
 *
 * Wraps an inner scrolling canvas (a private nested class) that paints the
 * messages — the canvas does all of the scrolling, layout and painting.
 * Anything we want to overlay on the conversation (e.g. a floating action
 * toolbar) can sit as a sibling of the canvas inside this panel, so it is
 * not affected by the canvas's scrolling.
 *
 * **Owns:** the inner canvas (wx-parented).
 * **Owned by:** `AiChatPanel` (wx-parented).
 */
class AiChatView final : public wxPanel {
public:
    NO_COPY_AND_MOVE(AiChatView)

    AiChatView(wxWindow* parent, Context& ctx);
    ~AiChatView() override;

    /// Replace the whole conversation and re-render.
    void setMessages(std::vector<ChatViewMessage> messages);

    /// Re-read theme/keyword config, then re-lay and repaint.
    void refreshTheme();

private:
    class Canvas; ///< Inner scrolled paint surface; defined in the .cpp.

    // The action bar's events bubble up to whichever parent it is currently
    // attached to (the canvas, or this panel directly in detached mode). The
    // panel catches them centrally and forwards into the canvas, so dispatch
    // does not depend on the bar's current parent.
    void onCopyCode(wxCommandEvent& event);
    void onInsertCode(wxCommandEvent& event);
    void onRunCode(wxCommandEvent& event);
    void onBarLeave(wxCommandEvent& event);

    Unowned<Canvas> m_canvas; ///< The conversation scroll surface.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
