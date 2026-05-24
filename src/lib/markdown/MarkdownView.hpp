//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "markdown/MarkdownDocument.hpp"
#include "markdown/MarkdownImageCache.hpp"
#include "markdown/MarkdownLayout.hpp"
#include "markdown/MarkdownRenderer.hpp"

namespace fbide::markdown {

/// Fired when the user clicks a link inside a `MarkdownView`. The URL is
/// on the event's string field. Parents that bind this event and do NOT
/// call `Skip()` suppress the default browser-launch; calling `Skip()`
/// (or not binding) falls through to `wxLaunchDefaultBrowser`.
wxDECLARE_EVENT(MARKDOWN_LINK_CLICKED, wxCommandEvent);

/**
 * Read-only scrollable markdown viewer.
 *
 * Drop-in `wxPanel` that renders a single markdown document with the
 * full `MarkdownRenderer` pipeline — headings, lists, tables, code
 * fences, images (downloaded asynchronously via `MarkdownImageCache`),
 * links and the rest. No editing, no selection, no caret. Suitable for
 * help text, about content, release notes, embedded documentation.
 *
 * Usage:
 *
 *     auto* view = new MarkdownView(parent);
 *     view->setMarkdown("# Hello\n\nSome **bold** text.");
 *     parent->GetSizer()->Add(view, wxSizerFlags(1).Expand());
 *
 * Link clicks emit `MARKDOWN_LINK_CLICKED` with the URL on the event's
 * string field; bind it on the view (or any parent up the chain) to
 * intercept. If no handler claims the event, `wxLaunchDefaultBrowser`
 * runs as a default.
 *
 * Optionally inject a custom fence highlighter via `setHighlighter` or
 * a shared / pre-configured image cache via `setImageCache`. The
 * default is a single-colour, monospace highlighter and a per-view
 * image cache that cleans up on destruction.
 */
class MarkdownView final : public wxScrolled<wxPanel> {
public:
    NO_COPY_AND_MOVE(MarkdownView)

    explicit MarkdownView(wxWindow* parent, wxWindowID winid = wxID_ANY);
    ~MarkdownView() override;

    /// Replace the rendered document. Triggers re-parse + re-layout +
    /// repaint. Cheap to call with unchanged text (the document's
    /// internal cache makes that a no-op).
    void setMarkdown(const wxString& markdown);

    [[nodiscard]] auto markdown() const -> const wxString& { return m_markdown; }

    /// Inject a custom code-fence highlighter. Pass an empty function
    /// to fall back to the built-in plain highlighter.
    void setHighlighter(CodeFenceHighlighter highlighter);

    /// Current selection — anchor + caret; empty when nothing is
    /// highlighted. Exposed for hosts that want to peek at / extract
    /// the selected text programmatically.
    [[nodiscard]] auto selection() const -> const Selection& { return m_selection; }

    /// Clear the current selection. No-op when already empty. Refreshes
    /// the view if anything changed.
    void clearSelection();

    /// Copy the currently selected text to the clipboard as plain text.
    /// No-op when the selection is empty.
    void copySelectionToClipboard() const;

    /// Replace the inline-image cache. The view owns one by default;
    /// supply a shared cache (e.g. one held across several views) to
    /// override. Passing `nullptr` restores the per-view default.
    void setImageCache(std::unique_ptr<MarkdownImageCache> cache);

    /// Re-resolve derived fonts from the current `GetFont()` and
    /// re-lay. Call after the host changes the view's font or after a
    /// system colour change.
    void refreshTheme();

    /// When `true` (default), code / patch lines soft-wrap to the
    /// available width. When `false` each source line stays on one
    /// PaintLine and an overflowing block scrolls horizontally — a
    /// thin scrollbar appears below each block whose content is wider
    /// than the visible area. Affects code blocks and SEARCH/REPLACE
    /// proposals only; prose / tables / images keep their behaviour.
    void setWrapCodeBlocks(bool wrap);
    [[nodiscard]] auto wrapCodeBlocks() const -> bool { return m_wrapCodeBlocks; }

private:
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onMotion(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onLeftDClick(wxMouseEvent& event);
    void onMouseWheel(wxMouseEvent& event);
    void onCharHook(wxKeyEvent& event);

    /// Locate the (line, run, char) the client point lands on. Snaps to
    /// the nearest line above / below when the point is outside the
    /// laid content, so a drag past the top / bottom still has a sane
    /// caret position. Non-const because hit-testing constructs a
    /// `wxClientDC` to measure — the measurement cache mutates as it
    /// fills in.
    [[nodiscard]] auto hitTest(const wxPoint& clientPoint) -> SelectionPosition;

    void relayout();
    /// Invalidate the document's cached layout, re-lay, and refresh.
    /// Used by paths that change a layout input (palette, highlighter,
    /// wrap mode, fonts) without changing the source text.
    void rebuild();
    /// Paint the per-block horizontal scrollbar overlay on top of any
    /// non-wrapped code / patch blocks visible in `update`. Drawn after
    /// the line pass so the thumb / track sit over the block's bottom
    /// padding strip. No-op for fully-wrapped layouts.
    void paintScrollbars(wxGCDC& gc, const wxRect& update,
        int contentTop, int contentLeft, int contentWidth,
        const MarkdownPalette& palette) const;
    void resolveFonts();
    /// Bind the cache's "ready" callback so a finished image download
    /// schedules a coalesced relayout. Re-invoked whenever the cache
    /// itself is swapped via `setImageCache`.
    void installImageCacheListener();
    [[nodiscard]] static auto palette() -> MarkdownPalette;
    [[nodiscard]] auto linkAt(const wxPoint& clientPoint) const -> wxString;

    /// Per-block horizontal-scroll-bar hit test. Result identifies the
    /// scroll block (index into `LaidOutDoc::scrollBlocks`) and exposes
    /// the scrollbar geometry so callers can compare against the thumb
    /// position. Empty when the click misses every scrollbar.
    struct ScrollbarTarget {
        std::size_t blockIndex = 0;
        int maxScroll = 0;
        int trackX = 0;
        int trackY = 0;
        int trackW = 0;
        int thumbX = 0;
        int thumbW = 0;
    };
    [[nodiscard]] auto scrollbarAt(const wxPoint& clientPoint) -> std::optional<ScrollbarTarget>;
    /// Current horizontal-scroll offset for a scroll block.
    [[nodiscard]] auto blockScrollOffset(std::size_t index) const -> int;
    /// Clamp + write a new scroll offset. No-op when unchanged.
    void setBlockScrollOffset(std::size_t index, int offset);

    wxString m_markdown; ///< Source text. `setMarkdown` writes this; `relayout` reads it.
    MarkdownDocument m_document;
    std::unique_ptr<MarkdownImageCache> m_imageCache;
    CodeFenceHighlighter m_highlighter;
    mutable std::vector<MeasurementEntry> m_measurerCache;

    wxBitmap m_buffer;   ///< Off-screen paint buffer, reused across paints.
    wxFont m_bodyFont;   ///< Base prose font (system GUI font by default).
    wxFont m_monoFont;   ///< Inline `code` and untagged fenced blocks.
    wxFont m_themedFont; ///< Themed code (unused by the default highlighter).
    int m_layoutWidth = -1;
    int m_bodyLineHeight = 0; ///< Body line-height — drives per-notch wheel scroll.
    int m_wheelPixelAccum = 0;
    bool m_imageRelayoutPending = false;
    Selection m_selection;        ///< Current rendered-text selection.
    bool m_dragSelecting = false; ///< True while the left mouse button is held during a drag.

    bool m_wrapCodeBlocks = true;     ///< Layout mode for code / patch blocks.
    std::vector<int> m_blockScroll;   ///< Per-scrollBlocks horizontal scroll offset (px).
    int m_dragScrollBlockIndex = -1;  ///< Block being scroll-dragged, or -1.
    int m_dragScrollStartOffset = 0;  ///< Scroll offset at drag start.
    int m_dragScrollStartMouseX = 0;  ///< Client-x at drag start.
    int m_hoverScrollBlockIndex = -1; ///< Block whose scrollbar the pointer hovers, or -1.
    int m_hwheelPixelAccum = 0;       ///< Horizontal-wheel fractional carry.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide::markdown
