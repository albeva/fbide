//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <unordered_set>
#include <wx/scrolwin.h>
#include "CodeActionBar.hpp"
#include "markdown/MarkdownDocument.hpp"
#include "markdown/MarkdownImageCache.hpp"
#include "markdown/MarkdownLayout.hpp"
#include "markdown/MarkdownRenderer.hpp"

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
 * AI conversation view.
 *
 * A `wxScrolled<wxWindow>` that owns the conversation model, paints the
 * messages and manages a floating code action bar shown over the hovered
 * code block. The action bar is a child of this window, so it scrolls
 * with the content.
 *
 * **Owns:** the action bar and the code highlighter.
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
    /// A message laid out inside its bubble. `document` owns the parsed +
    /// laid-out state and skips a re-layout when text + width match. The
    /// raw markdown lives inside it (`document.markdown()`).
    struct LaidMessage {
        markdown::MarkdownDocument document; ///< Parsed + laid markdown for this bubble.
        wxRect bubble;                       ///< Bubble rect in document coordinates.
        int contentWidth = 0;                ///< Content width inside the bubble padding.
        bool fromUser = false;               ///< Role — drives bubble colour + side.
    };

    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onMotion(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onLeftDClick(wxMouseEvent& event);
    void onCharHook(wxKeyEvent& event);
    void onLeaveWindow(wxMouseEvent& event);

    /// Locate which bubble (if any) the client point falls inside and
    /// hit-test inside its laid document. Returns `{messageIndex, pos}`
    /// where `messageIndex == -1` when the point is not inside a bubble.
    [[nodiscard]] auto hitTestBubble(const wxPoint& clientPoint) -> std::pair<int, markdown::SelectionPosition>;

    /// Hit-test within the bubble at `messageIndex` regardless of where
    /// the point actually is — used to extend a drag-selection cleanly
    /// when the pointer crosses out of the originating bubble.
    [[nodiscard]] auto hitTestInBubble(std::size_t messageIndex, const wxPoint& clientPoint) -> markdown::SelectionPosition;

    void clearSelection();
    void copySelectionToClipboard();
    void onScroll(wxScrollWinEvent& event);
    /// Pixel-precise wheel handler. Bypasses `wxScrolled`'s default
    /// line-quantising path, which combined with our 1-px scroll rate
    /// (`SetScrollRate(0, 1)`) would scroll only ~3 px per wheel notch.
    /// Converts rotation directly to pixels with a fractional carry so
    /// macOS trackpad momentum tails don't get rounded away.
    void onMouseWheel(wxMouseEvent& event);

    // Action-bar events propagate up the parent chain and land here.
    void onCopyCode(wxCommandEvent& event);
    void onInsertCode(wxCommandEvent& event);
    void onRunCode(wxCommandEvent& event);
    void onApplyPatch(wxCommandEvent& event);
    void onRejectPatch(wxCommandEvent& event);
    void onBarLeave(wxCommandEvent& event);

    /// Re-lay every message for the current view width.
    void relayout();

    /// Paint one laid-out message — its bubble and content. `pal` is hoisted
    /// from `onPaint` so it is resolved once per paint, not per bubble.
    /// `originY` is the scroll offset; `updateTop` / `updateBottom` bound the
    /// dirty band so lines outside it are skipped.
    void paintMessage(
        wxGCDC& gc,
        const LaidMessage& message,
        std::size_t messageIndex,
        const markdown::MarkdownPalette& pal,
        const markdown::TextMeasurer& measurer,
        int originY,
        int updateTop,
        int updateBottom
    ) const;

    /// Link target under a client point, or empty when none.
    [[nodiscard]] auto linkAt(const wxPoint& clientPoint) const -> wxString;

    /// (message, code-block) indices of the code block under a client point,
    /// or {-1, -1} when none.
    [[nodiscard]] auto codeBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int>;

    /// (message, patch-block) indices of the SEARCH/REPLACE proposal under
    /// a client point, or {-1, -1} when none.
    [[nodiscard]] auto patchBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int>;

    /// Show the action bar over block `blockIndex` of `messageIndex`, with
    /// `mode` selecting which button set the bar presents. Bar tracks the
    /// block while its top edge is visible, then pins to the top of the
    /// visible area once the block has scrolled past. A negative index
    /// hides it.
    void showActionBar(int messageIndex, int blockIndex, CodeActionBar::Mode mode);
    void hideActionBar();

    [[nodiscard]] auto palette() const -> markdown::MarkdownPalette;
    [[nodiscard]] auto bubbleColour(bool fromUser) const -> wxColour;

    /// Rebuild the cached bubble brushes from current system + theme colours.
    /// Called from the constructor and from `refreshTheme()`.
    void rebuildBubbleBrushes();

    /// Highlight a fenced code block — FreeBASIC through the lexer, anything
    /// else as plain default-coloured lines. `reformat` re-indents/reformats
    /// FreeBASIC code (used for model replies, not the user's own).
    [[nodiscard]] auto highlightFence(const wxString& code, const wxString& lang, bool reformat) const
        -> std::vector<CodeLine>;

    /// Resolve `m_bodyFont` / `m_monoFont` / `m_themedFont` from the
    /// platform default + the optional `[ai] fontSize` config override.
    /// All three share the resolved point size so faces line up.
    void resolveFonts();

    /// Apply a parsed SEARCH/REPLACE block to the active document.
    /// Returns `true` on success, `false` when the SEARCH text could not
    /// be located (e.g. the buffer changed since the proposal arrived).
    /// Wraps a single Scintilla undo action so one Ctrl-Z reverts it.
    auto applyPatch(const markdown::LaidPatchBlock& patch) -> bool;

    /// Walk every laid patch block and apply any not previously seen
    /// (success or failure both counted). Driven by the live-edit toggle
    /// from `setMessages` — every chunk reparse rebuilds the doc, so we
    /// need a stable key to skip already-handled blocks.
    void autoApplyPatches();

    Context& m_ctx;                                             ///< Application context.
    std::unique_ptr<CodeHighlighter> m_highlighter;             ///< FreeBASIC code highlighter.
    std::unique_ptr<markdown::MarkdownImageCache> m_imageCache; ///< Inline-image download cache.
    Unowned<CodeActionBar> m_actionBar;                         ///< Floating per-code-block toolbar.
    wxBitmap m_buffer;                                          ///< Off-screen paint buffer, reused across paints.
    wxFont m_bodyFont;                                          ///< Base prose font.
    wxFont m_monoFont;                                          ///< System monospace face — inline `code` and non-FB fences.
    wxFont m_themedFont;                                        ///< Editor-theme font resized to body — FreeBASIC fenced runs only.
    wxBrush m_userBubbleBrush;                                  ///< Cached bubble fill — user messages.
    wxBrush m_assistantBubbleBrush;                             ///< Cached bubble fill — assistant messages.
    std::vector<ChatViewMessage> m_messages;                    ///< Conversation source.
    std::vector<LaidMessage> m_items;                           ///< One laid-out bubble per message.
    int m_layoutWidth = -1;                                     ///< View width m_items were built for.
    int m_totalHeight = 0;                                      ///< Stacked height of all bubbles.
    int m_barMessage = -1;                                      ///< Message the action bar targets.
    int m_barIndex = -1;                                        ///< Code or patch block index — see `m_actionBar->mode()`.
    int m_selectionMessage = -1;                                ///< Bubble whose text is currently selected, or -1.
    markdown::Selection m_selection;                            ///< Selection within that bubble.
    bool m_dragSelecting = false;
    std::unordered_set<std::string> m_appliedPatches; ///< UTF-8 keys of patches already
                                                      ///< auto-applied (or attempted) this
                                                      ///< session — guards live-edit against
                                                      ///< double-apply across reparses.
    int m_bodyLineHeight = 0;                         ///< Body-font line height — sets the per-notch wheel scroll amount.
    int m_wheelPixelAccum = 0;                        ///< Fractional remainder carried between wheel events.
    /// True when an image-cache "ready" notification has already scheduled
    /// a deferred relayout via `CallAfter`. Subsequent notifications in
    /// the same event-loop tick coalesce into the pending one instead of
    /// stacking up additional relayout + repaint cycles.
    bool m_imageRelayoutPending = false;
    /// Persistent measurement cache shared across `DcMeasurer` instances.
    /// Cleared from `resolveFonts` / `refreshTheme` whenever any cached
    /// font would become stale. `mutable` so the const measurement path
    /// can populate it on first miss.
    mutable std::vector<markdown::MeasurementEntry> m_measurerCache;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
