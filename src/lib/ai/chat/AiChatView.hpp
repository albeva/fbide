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
        wxString markdown;     ///< Source markdown — layout cache key.
        int contentWidth = 0;  ///< Content width inside the bubble padding.
        bool fromUser = false; ///< Role — drives bubble colour + side.
    };

    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onMotion(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeaveWindow(wxMouseEvent& event);
    void onBarLeave(wxCommandEvent& event);   ///< EVT_CODE_BAR_LEAVE from the bar.
    void onCopyCode(wxCommandEvent& event);   ///< EVT_BUTTON, ID_CodeCopy.
    void onInsertCode(wxCommandEvent& event); ///< EVT_BUTTON, ID_CodeInsert.
    void onRunCode(wxCommandEvent& event);    ///< EVT_BUTTON, ID_CodeRun.
    void onScroll(wxScrollWinEvent& event);   ///< Reposition the bar on scroll.
    void onMouseWheel(wxMouseEvent& event);   ///< Reposition the bar on wheel.

    /// Re-lay every message for the current client width.
    void relayout();

    /// Paint one laid-out message — its bubble and content. `originY` is the
    /// scroll offset, so document coordinates map to client coordinates;
    /// `updateTop` / `updateBottom` bound the dirty band in client coordinates
    /// so lines outside it are skipped.
    void paintMessage(
        wxGCDC& gc, const LaidMessage& message, int originY, int updateTop, int updateBottom
    ) const;

    /// Link target under a client point, or empty when there is none.
    [[nodiscard]] auto linkAt(const wxPoint& clientPoint) const -> wxString;

    /// (message, code-block) indices of the code block under a client point,
    /// or {-1, -1} when the point is over no code block.
    [[nodiscard]] auto codeBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int>;

    /// Show the action bar over code block `codeIndex` of `messageIndex`;
    /// passing a negative index hides it.
    void showActionBar(int messageIndex, int codeIndex);
    void hideActionBar();

    /// Place the bar at its sticky position over its current target code
    /// block: anchored to the block's top-right when the top is visible,
    /// clamped to the viewport top while the block extends above the view.
    /// Hidden when the block is entirely outside the viewport.
    void repositionActionBar();

    /// Resolve the layout palette from the active theme + system colours.
    [[nodiscard]] auto palette() const -> ChatPalette;

    /// Bubble background colour for a user / assistant message.
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
    int m_layoutWidth = -1;                         ///< Client width m_items were built for.
    int m_totalHeight = 0;                          ///< Stacked height of all bubbles.
    int m_barMessage = -1;                          ///< Message the action bar targets.
    int m_barCode = -1;                             ///< Code block the action bar targets.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
