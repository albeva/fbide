//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ActionBarPlacement.hpp"
#include "BlockScrollController.hpp"
#include "ChatSelection.hpp"
#include "CodeActionBar.hpp"
#include "markdown/MarkdownDocument.hpp"
#include "markdown/MarkdownImageCache.hpp"
#include "markdown/MarkdownLayout.hpp"
#include "markdown/MarkdownRenderer.hpp"

class wxGCDC;

namespace fbide {
class Context;
namespace markdown {
    class CodeHighlighter;
}
} // namespace fbide

namespace fbide::ai {
class AiManager;

/// One conversation message handed to the chat view.
struct ChatViewMessage {
    bool fromUser = false;  ///< true = user prompt, false = assistant reply.
    bool streaming = false; ///< true while this bubble is the live reply being filled in.
    wxString markdown;      ///< Message body — markdown source.
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

    AiChatView(wxWindow* parent, Context& ctx, AiManager& manager);
    ~AiChatView() override;

    /// Replace the whole conversation and re-render.
    void setMessages(std::vector<ChatViewMessage> messages);

    /// Re-read theme/keyword config, then re-lay and repaint.
    void refreshTheme();

    /// Drop every cached layout and repaint — call after a chat-side
    /// policy input that the layout cache key doesn't capture (most
    /// importantly the live-edit toggle, since the collapse predicate
    /// reads `AiManager::isLiveEdit()` directly).
    void refreshCollapsePolicy();

private:
    /// A message laid out inside its bubble. `document` owns the parsed +
    /// laid-out state and skips a re-layout when text + width match. The
    /// raw markdown lives inside it (`document.markdown()`).
    struct LaidMessage {
        markdown::MarkdownDocument document; ///< Parsed + laid markdown for this bubble.
        wxRect bubble;                       ///< Bubble rect in document coordinates.
        int contentWidth = 0;                ///< Content width inside the bubble padding.
        bool fromUser = false;               ///< Role — drives bubble colour + side.
        bool streaming = false;              ///< Bubble is the in-flight reply being filled in.
        /// Horizontal scroll offset in pixels per non-wrapped scroll
        /// block. Indices line up with the laid doc's `scrollBlocks`.
        /// Empty when `wrapCodeBlocks` is true (or no such blocks
        /// exist in this bubble).
        std::vector<int> blockScroll;
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
    void onBlockCollapse(wxCommandEvent& event);
    void onBlockExpand(wxCommandEvent& event);
    void onBarLeave(wxCommandEvent& event);

    /// Toggle the user's collapse override for the anchored block and
    /// re-lay it. Both action-bar handlers share this — the only
    /// difference is the target state.
    void setAnchoredBlockCollapsed(bool collapsed);

    /// Resolve the effective collapse state for a fenced code / patch
    /// block. Applies the chat's policy (patches collapse when
    /// live-edit is on OR when the patch is already applied; code
    /// blocks stay expanded unless the user toggled them).
    [[nodiscard]] auto isBlockCollapsed(
        markdown::LaidScrollBlock::Kind kind,
        const wxString& lang,
        const wxString& contentA,
        const wxString& contentB
    ) const -> bool;

    /// Stable hash for a block's content. Patches use the
    /// `(search, replace)` pair (matches `AiManager::patchKey` so the
    /// applied-set lookups line up). Code blocks fold `(text, lang)`.
    [[nodiscard]] static auto blockKey(
        markdown::LaidScrollBlock::Kind kind,
        const wxString& lang,
        const wxString& contentA,
        const wxString& contentB
    ) -> std::size_t;

    /// `blockKey` variant taking pre-hashed content. Lets a caller holding a
    /// laid `LaidScrollBlock` rebuild the key from its stored `codeContentHash`
    /// (== hash of the code text) without keeping the verbatim text around.
    [[nodiscard]] static auto blockKey(
        markdown::LaidScrollBlock::Kind kind,
        const wxString& lang,
        std::size_t contentHashA,
        std::size_t contentHashB
    ) -> std::size_t;

    /// Resolve a fence-language tag (`freebasic`, `json`, …) or a
    /// patch target path (`editor.bas`) to its user-facing display
    /// name (`FreeBASIC`, `JSON`). Looks up the type's localised
    /// string via `ctx.tr("statusbar.type.<key>")` — same path the
    /// status bar uses, so the names stay in sync.
    ///
    /// For patches with an empty `lang` (the SEARCH header carried no
    /// target), falls back to the manager's pinned edit-target file
    /// extension — patches in agent mode apply against that file, so
    /// labelling them with its filetype is meaningful.
    ///
    /// Returns empty when nothing resolves; the painter then drops
    /// the wide-tier summary rather than echoing a raw tag.
    [[nodiscard]] auto resolveLanguageDisplayName(
        markdown::LaidScrollBlock::Kind kind,
        const wxString& lang
    ) const -> wxString;

    /// Force every laid document to re-run its layout pass on the next
    /// `relayout()` call. Used after a collapse-policy input changes —
    /// the cache key doesn't capture predicate state, so a manual
    /// invalidation is the cleanest way to keep paints in sync.
    void invalidateAllLayouts();

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

    /// Hit-test against per-block horizontal scrollbars. Returns the
    /// bubble + block + scrollbar geometry the click landed on, or
    /// empty when no scrollbar is at `clientPoint`. Used both for
    /// click-to-drag dispatch and for shift+wheel routing.
    struct ScrollbarTarget {
        std::size_t messageIndex = 0;
        std::size_t blockIndex = 0;
        int maxScroll = 0;
        int trackX = 0;
        int trackY = 0;
        int trackW = 0;
        int thumbX = 0;
        int thumbW = 0;
    };
    [[nodiscard]] auto scrollbarAt(const wxPoint& clientPoint) -> std::optional<ScrollbarTarget>;
    /// Find the (message, blockIndex) of an overflowing non-wrapped
    /// scroll block whose body the point lies inside. Used by
    /// shift+wheel to route horizontal scroll. Empty when no such
    /// block contains the point.
    struct BlockTarget {
        std::size_t messageIndex = 0;
        std::size_t blockIndex = 0;
    };
    [[nodiscard]] auto overflowingBlockAt(const wxPoint& clientPoint) -> std::optional<BlockTarget>;
    /// Clamp + write a per-block scroll offset and request a repaint.
    void setBlockScrollOffset(std::size_t messageIndex, std::size_t index, int offset);

    /// Show the action bar over block `blockIndex` of `messageIndex`,
    /// with `buttons` selecting which icons appear. The host derives
    /// the mask from the block's kind + collapse state + 1-liner-ness;
    /// the bar just shows whatever bits are set. Bar tracks the block
    /// while its top edge is visible, then pins to the top of the
    /// visible area once the block has scrolled past. A negative
    /// index hides it.
    void showActionBar(int messageIndex, int blockIndex, std::uint8_t buttons);

    /// Compute the `CodeActionBar::Button` bitmask for the laid block
    /// at `(mi, bi)`. Encapsulates the policy ("collapsed strips
    /// show only Expand; one-liners get no toggle at all") so the
    /// hover dispatch and the collapse / expand handlers stay in
    /// sync without each re-deriving the rules.
    [[nodiscard]] auto buttonsFor(std::size_t mi, std::size_t bi) const -> std::uint8_t;

    /// True when the laid block holds enough content that hiding its
    /// body actually saves screen real-estate. Strict 1-line code
    /// fences and `1+1`-line patches return false — collapsing them
    /// trades a one-line body for a one-line strip, so the toggle is
    /// hidden in the action bar.
    [[nodiscard]] static auto isCollapsibleBlock(const markdown::LaidScrollBlock& block) -> bool;
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
        -> std::vector<markdown::CodeLine>;

    /// Resolve `m_bodyFont` / `m_monoFont` / `m_themedFont` from the
    /// platform default + the optional `[ai] fontSize` config override.
    /// All three share the resolved point size so faces line up.
    void resolveFonts();

    /// Walk every laid patch block and ask `AiManager` to apply any
    /// not previously attempted. Driven by the live-edit toggle from
    /// `setMessages` — every chunk reparse rebuilds the doc, so the
    /// manager's applied-set is what skips already-handled blocks.
    void autoApplyPatches();

    Context& m_ctx;                                             ///< Application context.
    AiManager& m_manager;                                       ///< Owning chat tab's AI manager.
    std::unique_ptr<markdown::CodeHighlighter> m_highlighter;   ///< FreeBASIC code highlighter.
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
    ActionBarPlacement m_barPlacement;                          ///< Action bar anchor + placement math.
    ChatSelection m_selection;                                  ///< Selection state across the conversation.

    bool m_wrapCodeBlocks = true; ///< Cached `[markdown] wrapCodeBlocks` value; passed to layouts.

    /// Per-block horizontal scrollbar state (drag, hover, h-wheel
    /// accumulator) — see BlockScrollController.
    BlockScrollController m_blockScroll;

    int m_bodyLineHeight = 0;  ///< Body-font line height — sets the per-notch wheel scroll amount.
    int m_wheelPixelAccum = 0; ///< Fractional remainder carried between wheel events (vertical).
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

    /// Per-block user override for the collapsed-vs-expanded state.
    /// Keyed by content hash (`blockKey`) so the toggle survives
    /// re-layouts and applies to identical blocks across re-renders.
    /// Absent key = follow the default policy; present key wins.
    std::unordered_map<std::size_t, bool> m_collapseOverrides;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide::ai
