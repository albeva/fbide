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
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);

    /// Re-lay every message for the current client width.
    void relayout();

    /// Paint one laid-out message; `screenTop` is its top in client coords.
    void paintMessage(wxGCDC& gc, const LaidOutDoc& doc, int leftMargin, int screenTop) const;

    /// Resolve the layout palette from the active theme + system colours.
    [[nodiscard]] auto palette() const -> ChatPalette;

    /// Highlight a fenced code block — FreeBASIC through the lexer, anything
    /// else as plain default-coloured lines.
    [[nodiscard]] auto highlightFence(const wxString& code, const wxString& lang) const
        -> std::vector<CodeLine>;

    Context& m_ctx;                                 ///< Application context.
    std::unique_ptr<CodeHighlighter> m_highlighter; ///< FreeBASIC code highlighter.
    wxFont m_bodyFont;                              ///< Base prose font.
    wxFont m_monoFont;                              ///< Base monospace (code) font.
    std::vector<ChatViewMessage> m_messages;        ///< Conversation source.
    std::vector<LaidOutDoc> m_layouts;              ///< One layout per message.
    std::vector<int> m_offsets;                     ///< Top y of each message.
    int m_layoutWidth = -1;                         ///< Content width m_layouts were built for.
    int m_totalHeight = 0;                          ///< Stacked height of all messages.
};

} // namespace fbide
