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
class CodeActionBar;
class CodeHighlighter;

/// One conversation message handed to the chat view.
struct ChatViewMessage {
    bool fromUser = false; ///< true = user prompt, false = assistant reply.
    wxString markdown;     ///< Message body — markdown source.
};

/**
 * AI conversation view.
 *
 * A `wxPanel` that hosts an inner `wxScrolled<wxWindow>` as the scroll
 * surface. AiChatView itself owns the conversation model, paints the
 * messages into the scroll surface and manages the floating code action
 * bar. Anything we need to overlay on the conversation (the action bar)
 * sits as a sibling of the scroll surface inside this panel, so it can
 * stay put while the surface scrolls.
 *
 * **Owns:** the scroll surface, the action bar and the code highlighter.
 * **Owned by:** `AiChatPanel` (wx-parented).
 * **Threading:** UI thread only.
 */
class AiChatView final : public wxScrolled<wxWindow> {
public:
    NO_COPY_AND_MOVE(AiChatView)

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
        wxString markdown;     ///< Source markdown — layout cache key.
        int contentWidth = 0;  ///< Content width inside the bubble padding.
        bool fromUser = false; ///< Role — drives bubble colour + side.
    };

    // Scroll-surface events arrive here via the pushed handler on
    // `m_scrollView` — see the ctor.
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onMotion(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeaveWindow(wxMouseEvent& event);
    void onScroll(wxScrollWinEvent& event);

    // Action-bar events propagate up the parent chain and land here.
    void onCopyCode(wxCommandEvent& event);
    void onInsertCode(wxCommandEvent& event);
    void onRunCode(wxCommandEvent& event);
    void onBarLeave(wxCommandEvent& event);

    /// Re-lay every message for the current scroll-surface width.
    void relayout();

    /// Paint one laid-out message — its bubble and content. `originY` is the
    /// scroll offset; `updateTop` / `updateBottom` bound the dirty band so
    /// lines outside it are skipped.
    void paintMessage(
        wxGCDC& gc, const LaidMessage& message, int originY, int updateTop, int updateBottom
    ) const;

    /// Link target under a scroll-surface client point, or empty when none.
    [[nodiscard]] auto linkAt(const wxPoint& clientPoint) const -> wxString;

    /// (message, code-block) indices of the code block under a scroll-surface
    /// client point, or {-1, -1} when none.
    [[nodiscard]] auto codeBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int>;

    /// Show the action bar over code block `codeIndex` of `messageIndex`,
    /// in attached or detached mode as the snippet's top edge dictates.
    /// A negative index hides it.
    void showActionBar(int messageIndex, int codeIndex);
    void hideActionBar();

    [[nodiscard]] auto palette() const -> ChatPalette;
    [[nodiscard]] auto bubbleColour(bool fromUser) const -> wxColour;

    /// Highlight a fenced code block — FreeBASIC through the lexer, anything
    /// else as plain default-coloured lines. `reformat` re-indents/reformats
    /// FreeBASIC code (used for model replies, not the user's own).
    [[nodiscard]] auto highlightFence(const wxString& code, const wxString& lang, bool reformat) const
        -> std::vector<CodeLine>;

    Context& m_ctx;                                 ///< Application context.
    std::unique_ptr<CodeHighlighter> m_highlighter; ///< FreeBASIC code highlighter.
    Unowned<CodeActionBar> m_actionBar;             ///< Floating per-code-block toolbar.
    wxBitmap m_buffer;                              ///< Off-screen paint buffer, reused across paints.
    wxFont m_bodyFont;                              ///< Base prose font.
    wxFont m_monoFont;                              ///< Base monospace (code) font.
    std::vector<ChatViewMessage> m_messages;        ///< Conversation source.
    std::vector<LaidMessage> m_items;               ///< One laid-out bubble per message.
    int m_layoutWidth = -1;                         ///< Scroll-surface width m_items were built for.
    int m_totalHeight = 0;                          ///< Stacked height of all bubbles.
    int m_barMessage = -1;                          ///< Message the action bar targets.
    int m_barCode = -1;                             ///< Code block the action bar targets.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
