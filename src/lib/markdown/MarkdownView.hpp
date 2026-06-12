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

namespace fbide {
class Context;
} // namespace fbide

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

    /// `ctx` gives the view access to the editor config + theme so the
    /// palette (notably the fenced-code background) is themed by default,
    /// without the host wiring colours in. `style` is the `wxScrolled` window
    /// style (default `wxVSCROLL`); pass `0` to never show a scroll bar — e.g.
    /// when the host sizes the view to its content. `wxCLIP_CHILDREN` is always
    /// added.
    MarkdownView(wxWindow* parent, Context& ctx, wxWindowID winid = wxID_ANY, long style = wxVSCROLL);
    ~MarkdownView() override;

    /// Replace the rendered document. Triggers re-parse + re-layout +
    /// repaint. Cheap to call with unchanged text (the document's
    /// internal cache makes that a no-op).
    void setMarkdown(const wxString& markdown);

    [[nodiscard]] auto markdown() const -> const wxString& { return m_document.markdown(); }

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

    /// Table rendering style — bordered (default) or borderless with custom
    /// row / column spacing. See `MdTableStyle`.
    void setTableStyle(const MdTableStyle& style);
    [[nodiscard]] auto tableStyle() const -> const MdTableStyle& { return m_tableStyle; }

    /// Enable / disable text selection. When `false` the view is purely
    /// read-only: no drag-select, no double-click word select, no
    /// Ctrl+A / Ctrl+C, and a plain arrow cursor over text. Links still
    /// work. Disabling clears any current selection. Default: `true`.
    void setSelectable(bool selectable);
    [[nodiscard]] auto selectable() const -> bool { return m_selectable; }

    /// Background colour painted behind the content and used as the
    /// blend base for derived tints (code background, rules, table
    /// header). Pass an invalid colour to restore the default (the
    /// system window colour). Set this to the host's background to blend
    /// the view seamlessly into a dialog.
    void setContentBackground(const wxColour& colour);

    /// Padding (px) between the panel edge and the content, applied on
    /// all four sides. Default: 8. Set to 0 to render edge-to-edge.
    void setContentPadding(int padding);
    [[nodiscard]] auto contentPadding() const -> int { return m_contentPadding; }

    /// Override the body-text colour. Pass an invalid colour to restore the
    /// default (system window text). Lets a host with a custom content
    /// background keep prose legible.
    void setTextColour(const wxColour& colour);
    /// Override the hyperlink colour. Invalid colour restores the default
    /// (system hotlight).
    void setLinkColour(const wxColour& colour);

private:
    /// Best size for a host-pinned width: when the width is fixed via a min
    /// width, report the content height at that width (laid out with the real
    /// fonts) so a plain `(W, -1)` min size sizes the host through a single
    /// `Fit`. Falls back to the scrolled default when no min width is set.
    [[nodiscard]] auto DoGetBestSize() const -> wxSize override;

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

    /// Lay the document out at `contentWidth` (px, inside the padding) through
    /// the measurer + palette. Shared by `relayout` and the height-for-width
    /// query; touches neither the scroll state nor the virtual size.
    void layoutDocument(const wxString& source, int contentWidth);
    void relayout(const wxString& source);
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
    /// Rebuild `m_palette` from the current content background plus the
    /// editor theme (the fenced-code background comes from the theme).
    /// Centralised so every palette rebuild stays themed.
    void rebuildPalette();
    /// Effective content background — the host override when set, else
    /// the system window colour.
    [[nodiscard]] auto backgroundColour() const -> wxColour;
    /// Bind the cache's "ready" callback so a finished image download
    /// schedules a coalesced relayout. Re-invoked whenever the cache
    /// itself is swapped via `setImageCache`.
    void installImageCacheListener();
    [[nodiscard]] static auto palette(const wxColour& windowBg, const wxColour& codeBg) -> MarkdownPalette;
    [[nodiscard]] auto linkAt(const wxPoint& clientPoint) const -> wxString;
    /// Link id (index into `LaidOutDoc::links`) under `clientPoint`, or -1.
    /// `linkAt` maps the result to a URL; hover tracking keeps the raw id.
    [[nodiscard]] auto linkIdAt(const wxPoint& clientPoint) const -> int;

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

    Context& m_ctx;              ///< Editor config + theme — palette source.
    MarkdownDocument m_document; ///< Owns the source text + cached layout (single source of truth).
    std::unique_ptr<MarkdownImageCache> m_imageCache;
    CodeFenceHighlighter m_highlighter;
    mutable std::vector<MeasurementEntry> m_measurerCache;

    wxBitmap m_buffer;   ///< Off-screen paint buffer, reused across paints.
    wxFont m_bodyFont;   ///< Base prose font (system GUI font by default).
    wxFont m_monoFont;   ///< Inline `code` and untagged fenced blocks.
    wxFont m_themedFont; ///< Themed code (unused by the default highlighter).
    /// Palette + selection-highlight colour derived from system colours.
    /// Cached here (refreshed in `resolveFonts`) so `onPaint` / `relayout`
    /// don't rebuild them — they only change on a system theme change, which
    /// routes through `refreshTheme` → `resolveFonts`.
    MarkdownPalette m_palette;
    wxColour m_highlightColour;
    int m_layoutWidth = -1;
    int m_bodyLineHeight = 0; ///< Body line-height — drives per-notch wheel scroll.
    int m_wheelPixelAccum = 0;
    bool m_imageRelayoutPending = false;
    Selection m_selection;        ///< Current rendered-text selection.
    bool m_dragSelecting = false; ///< True while the left mouse button is held during a drag.
    bool m_selectable = true;     ///< When false, all selection paths are disabled.

    /// Host-supplied content background. Invalid (default) means follow
    /// the system window colour — see `backgroundColour`.
    wxColour m_backgroundColour;
    /// Host text / link colour overrides. Invalid (default) follows the system
    /// colours; set to stay legible on a custom content background.
    wxColour m_textColour;
    wxColour m_linkColour;
    static constexpr int kDefaultContentPadding = 8;
    int m_contentPadding = kDefaultContentPadding; ///< Inner padding between the panel edge and content.

    bool m_wrapCodeBlocks = true;     ///< Layout mode for code / patch blocks.
    MdTableStyle m_tableStyle;        ///< Bordered (default) or borderless table rendering.
    std::vector<int> m_blockScroll;   ///< Per-scrollBlocks horizontal scroll offset (px).
    int m_dragScrollBlockIndex = -1;  ///< Block being scroll-dragged, or -1.
    int m_dragScrollStartOffset = 0;  ///< Scroll offset at drag start.
    int m_dragScrollStartMouseX = 0;  ///< Client-x at drag start.
    int m_hoverScrollBlockIndex = -1; ///< Block whose scrollbar the pointer hovers, or -1.
    int m_hoveredLinkId = -1;         ///< Link the pointer hovers (drawn underlined), or -1.
    int m_hwheelPixelAccum = 0;       ///< Horizontal-wheel fractional carry.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide::markdown
