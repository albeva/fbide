//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/scrolwin.h>
#include "ChatLayout.hpp"

class wxGCDC;

namespace fbide {
class Context;
class CodeHighlighter;

/// One conversation message handed to the chat view.
struct ChatViewMessage {
    bool fromUser = false; ///< true = user prompt, false = assistant reply.
    wxString markdown;     ///< Message body — markdown source.
};

/**
 * Scrolled, custom-painted conversation view.
 *
 * Each message is laid out once (per width) by the markdown layout engine
 * into stacked, wrapped lines; `onPaint` walks only the lines inside the
 * viewport. No per-message child windows — the whole conversation is a
 * single scrolled window, so it stays light with hundreds of messages.
 *
 * **Owns:** its `CodeHighlighter`.
 * **Owned by:** `AiChatPanel` (wx-parented).
 * **Threading:** UI thread only.
 */
class AiChatView final : public wxScrolled<wxWindow> {
public:
    NO_COPY_AND_MOVE(AiChatView)

    /// Build the view as a child of `parent`.
    AiChatView(wxWindow* parent, Context& ctx);
    ~AiChatView() override;

    /// Replace the whole conversation and re-render.
    void setMessages(std::vector<ChatViewMessage> messages);

    /// Re-read theme/keyword config, then re-lay and repaint.
    void refreshTheme();

private:
    /// A message laid out inside its bubble.
    struct LaidMessage {
        LaidOutDoc doc;        ///< Wrapped content.
        wxRect bubble;         ///< Bubble rect in document coordinates.
        int contentWidth = 0;  ///< Content width inside the bubble padding.
        bool fromUser = false; ///< Role — drives bubble colour + side.
    };

    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);

    /// Re-lay every message for the current client width.
    void relayout();

    /// Paint one laid-out message — its bubble and content. `originY` is the
    /// scroll offset, so document coordinates map to client coordinates.
    void paintMessage(wxGCDC& gc, const LaidMessage& message, int originY) const;

    /// Resolve the layout palette from the active theme + system colours.
    [[nodiscard]] auto palette() const -> ChatPalette;

    /// Bubble background colour for a user / assistant message.
    [[nodiscard]] auto bubbleColour(bool fromUser) const -> wxColour;

    /// Highlight a fenced code block — FreeBASIC through the lexer, anything
    /// else as plain default-coloured lines.
    [[nodiscard]] auto highlightFence(const wxString& code, const wxString& lang) const
        -> std::vector<CodeLine>;

    Context& m_ctx;                                 ///< Application context.
    std::unique_ptr<CodeHighlighter> m_highlighter; ///< FreeBASIC code highlighter.
    wxFont m_bodyFont;                              ///< Base prose font.
    wxFont m_monoFont;                              ///< Base monospace (code) font.
    std::vector<ChatViewMessage> m_messages;        ///< Conversation source.
    std::vector<LaidMessage> m_items;               ///< One laid-out bubble per message.
    int m_layoutWidth = -1;                         ///< Client width m_items were built for.
    int m_totalHeight = 0;                          ///< Stacked height of all bubbles.
};

} // namespace fbide
