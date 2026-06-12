//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownView.hpp"
#include "app/Context.hpp"
#include "config/Theme.hpp"
using namespace fbide;
using namespace fbide::markdown;

// NOLINTNEXTLINE(cert-err58-cpp, bugprone-throwing-static-initialization)
wxDEFINE_EVENT(fbide::markdown::MARKDOWN_LINK_CLICKED, wxCommandEvent);

namespace {

/// A touch of leading on top of the measured font height — matches the
/// chat view's calculation so wheel-step quantisation lines up.
constexpr int kBodyLeading = 4;
/// Alpha for the translucent selection band so backgrounds bleed through.
constexpr unsigned char kHighlightAlpha = 100;

// Palette-derivation factors. The defaults are visually subtle blends
// against the system window background; named so the tidy magic-number
// check sees their intent.
constexpr double kCodeBgBlend = 0.07;
constexpr double kInlineCodeBgBlend = 0.10;
constexpr double kRuleBlend = 0.30;
constexpr double kTableHeaderBgBlend = 0.07;
constexpr double kPatchHalfBlend = 0.20;
// RGB triplets for the SEARCH (red) and REPLACE (green) tint targets.
// `wxColour(R, G, B)` — small enough to be obvious, named for clarity.
constexpr unsigned char kPatchRedR = 220;
constexpr unsigned char kPatchRedG = 80;
constexpr unsigned char kPatchRedB = 80;
constexpr unsigned char kPatchGreenR = 80;
constexpr unsigned char kPatchGreenG = 180;
constexpr unsigned char kPatchGreenB = 80;

// Per-block horizontal scrollbar — drawn at the bottom of an
// overflowing code or patch block when `wrapCodeBlocks = false`.
constexpr int kScrollbarHeight = 6;    ///< Track height in pixels.
constexpr int kScrollbarMinThumb = 24; ///< Floor so a tiny thumb stays grabbable.
constexpr unsigned char kScrollbarTrackAlpha = 60;
constexpr unsigned char kScrollbarThumbAlpha = 160;
constexpr unsigned char kScrollbarThumbActiveAlpha = 220;
constexpr int kScrollbarWheelStep = 40; ///< Pixels per shift-wheel notch.
constexpr int kHorizPxPerNotch = 60;    ///< Pixels per trackpad horizontal notch (with damper).

/// Linear blend of two colours — `factor` of 0 yields `from`, 1 yields `to`.
auto blend(const wxColour& from, const wxColour& to, const double factor) -> wxColour {
    const auto mix = [factor](const unsigned char src, const unsigned char dst) {
        return static_cast<unsigned char>(src + ((dst - src) * factor));
    };
    return { mix(from.Red(), to.Red()),
        mix(from.Green(), to.Green()),
        mix(from.Blue(), to.Blue()) };
}

/// Default highlighter — one CodeLine per source line, all rendered in
/// the system's default text colour, monospace. No syntax colouring.
/// Hosts that want language-aware highlighting inject one via
/// `MarkdownView::setHighlighter`.
auto defaultHighlight(const wxString& code, const wxString& /*lang*/) -> std::vector<CodeLine> {
    return plainCodeLines(code);
}

/// Pick a graphics renderer. Same Direct2D-on-Windows preference as the
/// chat view — required for colour emoji on Windows; falls through to
/// the default renderer everywhere else.
template<typename DcT>
auto makeGraphicsContext(DcT& target) -> wxGraphicsContext* {
    // CreateContext is non-const in the wx API; tidy can't see that
    // through the virtual overload set, so the const-correctness check
    // would otherwise flag this pointer.
    // NOLINTNEXTLINE(misc-const-correctness)
    wxGraphicsRenderer* renderer = nullptr;
#ifdef __WXMSW__
    renderer = wxGraphicsRenderer::GetDirect2DRenderer();
#endif
    if (renderer == nullptr) {
        renderer = wxGraphicsRenderer::GetDefaultRenderer();
    }
    return renderer->CreateContext(target);
}

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(MarkdownView, wxScrolled<wxPanel>)
    EVT_PAINT(MarkdownView::onPaint)
    EVT_SIZE(MarkdownView::onSize)
    EVT_MOTION(MarkdownView::onMotion)
    EVT_LEFT_DOWN(MarkdownView::onLeftDown)
    EVT_LEFT_UP(MarkdownView::onLeftUp)
    EVT_LEFT_DCLICK(MarkdownView::onLeftDClick)
    EVT_MOUSEWHEEL(MarkdownView::onMouseWheel)
    EVT_CHAR_HOOK(MarkdownView::onCharHook)
wxEND_EVENT_TABLE()
// clang-format on

MarkdownView::MarkdownView(wxWindow* parent, Context& ctx, const wxWindowID winid)
: wxScrolled(parent, winid, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_ctx(ctx)
, m_imageCache(std::make_unique<MarkdownImageCache>())
, m_highlighter(defaultHighlight) {
    wxScrolled::SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, 1);
    installImageCacheListener();
    resolveFonts();
    // Drop scrollbar hover when the pointer leaves so the active-thumb
    // tint doesn't linger. Bound here instead of in the static event
    // table because this is the only leave-window handler the view needs.
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) {
        if (m_hoverScrollBlockIndex >= 0) {
            m_hoverScrollBlockIndex = -1;
            Refresh();
        }
        event.Skip();
    });
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const wxString& markdown) {
    if (markdown == m_document.markdown()) {
        return;
    }
    // Content change invalidates any positions the selection held.
    m_selection.clear();
    relayout(markdown);
    Scroll(0, 0);
    Refresh();
}

void MarkdownView::setHighlighter(CodeFenceHighlighter highlighter) {
    m_highlighter = highlighter ? std::move(highlighter) : defaultHighlight;
    // The cached layout was built with the previous highlighter; force a
    // rebuild so code blocks pick up the new colouring. Any cached
    // selection positions belong to the old code-run layout.
    m_selection.clear();
    rebuild();
}

void MarkdownView::setImageCache(std::unique_ptr<MarkdownImageCache> cache) {
    m_imageCache = cache ? std::move(cache) : std::make_unique<MarkdownImageCache>();
    installImageCacheListener();
}

void MarkdownView::installImageCacheListener() {
    // Multiple downloads can settle in the same event-loop tick; coalesce
    // them into a single deferred relayout + repaint.
    m_imageCache->setListener([this](const wxString& /*url*/) {
        if (m_imageRelayoutPending) {
            return;
        }
        m_imageRelayoutPending = true;
        CallAfter([this] {
            m_imageRelayoutPending = false;
            relayout(m_document.markdown());
            Refresh();
        });
    });
}

void MarkdownView::setWrapCodeBlocks(const bool wrap) {
    if (wrap == m_wrapCodeBlocks) {
        return;
    }
    m_wrapCodeBlocks = wrap;
    // Wrap mode is a layout-time decision — invalidate so the next
    // relayout walks the source again. Per-block scroll state belongs
    // to the previous wrap mode; reset it.
    m_blockScroll.clear();
    rebuild();
}

void MarkdownView::setSelectable(const bool selectable) {
    if (selectable == m_selectable) {
        return;
    }
    m_selectable = selectable;
    // A live selection can't survive read-only mode — drop it.
    if (!m_selectable) {
        clearSelection();
    }
}

void MarkdownView::setContentBackground(const wxColour& colour) {
    m_backgroundColour = colour;
    // Mirror it onto the window so any wx-drawn edges match, then
    // re-derive the palette (its tints blend against this colour) and
    // re-lay so cached runs pick up the new background.
    SetBackgroundColour(backgroundColour());
    rebuildPalette();
    rebuild();
}

void MarkdownView::setTextColour(const wxColour& colour) {
    m_textColour = colour;
    rebuildPalette();
    rebuild();
}

void MarkdownView::setLinkColour(const wxColour& colour) {
    m_linkColour = colour;
    rebuildPalette();
    rebuild();
}

void MarkdownView::setContentPadding(const int padding) {
    const int clamped = std::max(0, padding);
    if (clamped == m_contentPadding) {
        return;
    }
    m_contentPadding = clamped;
    // Padding feeds the content width, so the document must re-wrap.
    relayout(m_document.markdown());
    Refresh();
}

auto MarkdownView::layoutHeight() -> int {
    // Re-lay at the current width and report the full content height so a host
    // can size itself to the content and avoid showing the scroll bar.
    relayout(m_document.markdown());
    return m_document.height() + (2 * m_contentPadding);
}

void MarkdownView::refreshTheme() {
    resolveFonts();
    m_measurerCache.clear();
    // Palette / fonts changed — every cached run is stale.
    rebuild();
}

void MarkdownView::rebuild() {
    m_document.invalidate();
    relayout(m_document.markdown());
    Refresh();
}

void MarkdownView::resolveFonts() {
    m_measurerCache.clear();
    m_bodyFont = GetFont();
    if (!m_bodyFont.IsOk()) {
        m_bodyFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    }
    const int size = m_bodyFont.GetPointSize();
    m_monoFont = wxFont(wxFontInfo(size).Family(wxFONTFAMILY_TELETYPE));
    m_themedFont = m_monoFont; // no editor theme available — fall back.

    const wxClientDC dc(this);
    wxCoord textWidth = 0;
    wxCoord textHeight = 0;
    dc.GetTextExtent("Ag", &textWidth, &textHeight, nullptr, nullptr, &m_bodyFont);
    m_bodyLineHeight = textHeight + kBodyLeading;

    // Palette (content background + system colours + editor theme) and
    // the selection-highlight colour are cached here so `onPaint` /
    // `relayout` don't rebuild them per call.
    rebuildPalette();
    const wxColour sysHighlight = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    m_highlightColour = wxColour(sysHighlight.Red(), sysHighlight.Green(), sysHighlight.Blue(), kHighlightAlpha);
}

auto MarkdownView::backgroundColour() const -> wxColour {
    return m_backgroundColour.IsOk()
             ? m_backgroundColour
             : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}

void MarkdownView::rebuildPalette() {
    // Fenced code sits on the editor theme's background so the syntax
    // colours render on their intended canvas rather than a system grey.
    m_palette = palette(backgroundColour(), m_ctx.getTheme().background({}));
    // Host overrides win — a custom content background (e.g. the About dialog's
    // brand blue) needs text / links the system colours wouldn't make legible.
    if (m_textColour.IsOk()) {
        m_palette.text = m_textColour;
    }
    if (m_linkColour.IsOk()) {
        m_palette.link = m_linkColour;
    }
}

auto MarkdownView::palette(const wxColour& windowBg, const wxColour& codeBg) -> MarkdownPalette {
    const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour patchRed { kPatchRedR, kPatchRedG, kPatchRedB };
    const wxColour patchGreen { kPatchGreenR, kPatchGreenG, kPatchGreenB };
    // `codeBg` is the editor theme's background (the canvas the syntax
    // colours were designed for); fall back to a system blend when the
    // theme leaves it unset so a fenced block never paints with an
    // invalid brush.
    const wxColour resolvedCodeBg = codeBg.IsOk() ? codeBg : blend(windowBg, windowText, kCodeBgBlend);
    return { .text = windowText,
        .link = wxSystemSettings::GetColour(wxSYS_COLOUR_HOTLIGHT),
        .codeBg = resolvedCodeBg,
        .inlineCodeBg = blend(windowBg, windowText, kInlineCodeBgBlend),
        .rule = blend(windowBg, windowText, kRuleBlend),
        .tableHeaderBg = blend(windowBg, windowText, kTableHeaderBgBlend),
        .patchSearchBg = blend(windowBg, patchRed, kPatchHalfBlend),
        .patchReplaceBg = blend(windowBg, patchGreen, kPatchHalfBlend),
        .patchFg = windowText,
        // No editor theme to source diff colours from — use the
        // built-in patch palette so collapsed strips still pick up
        // green / red git-style accents.
        .added = patchGreen,
        .removed = patchRed };
}

void MarkdownView::onSize(wxSizeEvent& event) {
    // A width change re-wraps every line; the cached `(line, run, char)`
    // positions in `m_selection` would then point into a different
    // distribution of runs. Convert the selection to flat
    // character offsets (stable across re-wrap), relayout, then convert
    // back — the highlight stays anchored on the same characters.
    const bool widthChanged = GetClientSize().GetWidth() != m_layoutWidth;
    std::size_t anchorOffset = 0;
    std::size_t caretOffset = 0;
    const bool remap = widthChanged && !m_selection.empty();
    if (remap) {
        const auto& laid = m_document.laid();
        anchorOffset = selectionToOffset(laid, m_selection.anchor);
        caretOffset = selectionToOffset(laid, m_selection.caret);
    }
    relayout(m_document.markdown());
    if (remap) {
        const auto& laid = m_document.laid();
        // Bias the lower offset toward the start of its line and the
        // higher toward the end so the selection's outer edges stick
        // where the user originally clicked.
        const bool anchorIsLow = anchorOffset <= caretOffset;
        m_selection.anchor = selectionFromOffset(laid, anchorOffset,
            anchorIsLow ? OffsetBias::PreferLineStart : OffsetBias::PreferLineEnd);
        m_selection.caret = selectionFromOffset(laid, caretOffset,
            anchorIsLow ? OffsetBias::PreferLineEnd : OffsetBias::PreferLineStart);
    }
    Refresh();
    event.Skip();
}

void MarkdownView::relayout(const wxString& source) {
    const int panelWidth = GetClientSize().GetWidth();
    if (panelWidth <= 0) {
        return;
    }
    const int contentWidth = std::max(40, panelWidth - (2 * m_contentPadding));

    const wxClientDC clientDc(this);
    wxGCDC measureDc;
    measureDc.SetGraphicsContext(makeGraphicsContext(clientDc));
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);

    const auto resolveImage = [this](const wxString& url) -> ImageInfo {
        const auto& entry = m_imageCache->get(url);
        ImageInfo info;
        info.bitmap = entry.bitmap;
        info.width = entry.width;
        info.height = entry.height;
        switch (entry.state) {
        case MarkdownImageCache::State::Ready:
            info.state = ImageInfo::State::Ready;
            break;
        case MarkdownImageCache::State::Failed:
            info.state = ImageInfo::State::Failed;
            break;
        case MarkdownImageCache::State::Loading:
            info.state = ImageInfo::State::Loading;
            break;
        }
        return info;
    };

    m_document.setMarkdown(source, contentWidth, measurer, m_palette, m_highlighter, resolveImage, m_wrapCodeBlocks);

    // Resize the per-block scroll vector to match the new layout. When
    // the block count is unchanged the previous offsets carry over
    // (clamped if the natural width shrank); when it changes the tail
    // / head defaults to zero.
    const auto& laid = m_document.laid();
    m_blockScroll.resize(laid.scrollBlocks.size(), 0);
    for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
        const auto& block = laid.scrollBlocks.at(i);
        const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
        m_blockScroll[i] = std::clamp(m_blockScroll[i], 0, maxScroll);
    }

    const int totalHeight = m_document.height() + (2 * m_contentPadding);
    SetVirtualSize(panelWidth, totalHeight);
    m_layoutWidth = panelWidth;
}

void MarkdownView::onPaint(wxPaintEvent& /*event*/) {
    wxPaintDC paintDc(this);
    const wxSize size = GetClientSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0) {
        return;
    }

    const double scale = GetDPIScaleFactor();
    if (!m_buffer.IsOk() || m_buffer.GetDIPSize() != size || m_buffer.GetScaleFactor() != scale) {
        m_buffer.CreateWithDIPSize(size, scale);
    }

    const wxRect update = GetUpdateRegion().GetBox();

    wxMemoryDC memoryDc(m_buffer);
    {
        wxGCDC gc;
        gc.SetGraphicsContext(makeGraphicsContext(memoryDc));

        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(backgroundColour()));
        gc.DrawRectangle(update);

        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int regionTopDoc = originY + update.y;
        const int regionBottomDoc = regionTopDoc + update.height;

        const MarkdownPalette& pal = m_palette;
        const auto& laid = m_document.laid();
        const int contentLeft = m_contentPadding;
        const int contentTop = m_contentPadding - originY;
        const int contentWidth = std::max(0, size.GetWidth() - (2 * m_contentPadding));

        const int regionTopRel = regionTopDoc - m_contentPadding;
        const int regionBottomRel = regionBottomDoc - m_contentPadding;
        const auto first = std::ranges::lower_bound(
            laid.lines, regionTopRel,
            [](const int height, const int top) { return height < top; },
            [](const PaintLine& line) { return line.y + line.height; }
        );

        // One measurer reused across the line loop — `paintSelectionHighlight`
        // needs per-character widths to compute the highlight rect, and a
        // fresh DcMeasurer + the shared cache is cheap to build.
        const DcMeasurer measurer(memoryDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);
        // Translucent selection (cached in `m_highlightColour`) so code /
        // patch / table backgrounds and inline images bleed through.
        const wxColour& highlightColour = m_highlightColour;

        PaintRunState runState;
        for (auto it = first; it != laid.lines.end(); ++it) {
            const auto& line = *it;
            if (line.y > regionBottomRel) {
                break;
            }
            const auto lineIdx = static_cast<std::size_t>(std::distance(laid.lines.begin(), it));
            const int lineTop = contentTop + line.y;
            const auto next = std::next(it);
            const int nextLineY = (next == laid.lines.end()) ? -1 : next->y;
            markdown::paintLineBackground(gc, line, contentLeft, lineTop, contentWidth, pal);

            // Code / patch lines belonging to a non-wrapped block paint
            // their text inside the block's content rect, shifted left
            // by the block's scroll offset. The clip stops the shifted
            // text from drawing under the bubble padding to either
            // side; the background was painted full-width above so
            // there's no gap.
            int scrollX = 0;
            bool clipped = false;
            if (line.blockIndex >= 0
                && static_cast<std::size_t>(line.blockIndex) < laid.scrollBlocks.size()) {
                const auto& block = laid.scrollBlocks.at(static_cast<std::size_t>(line.blockIndex));
                if (!block.wrapped) {
                    scrollX = m_blockScroll.at(static_cast<std::size_t>(line.blockIndex));
                    gc.SetClippingRegion(contentLeft + block.contentLeft, lineTop, block.contentWidth, line.height);
                    clipped = true;
                }
            }

            markdown::paintSelectionHighlight(gc, line, lineIdx, contentLeft - scrollX, lineTop, contentWidth, nextLineY, m_selection, highlightColour, measurer);
            markdown::paintLineText(gc, line, contentLeft - scrollX, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);

            if (clipped) {
                gc.DestroyClippingRegion();
                // PaintRunState caches the DC's current font/colour, but
                // the clip change above didn't touch those. Leave the
                // state intact so adjacent same-style runs still skip
                // the redundant SetFont calls.
            }
        }

        paintScrollbars(gc, update, contentTop, contentLeft, contentWidth, pal);

        gc.GetGraphicsContext()->Flush();
    }
    paintDc.Blit(update.x, update.y, update.width, update.height, &memoryDc, update.x, update.y);
}

void MarkdownView::paintScrollbars(
    wxGCDC& gc,
    const wxRect& update,
    const int contentTop,
    const int contentLeft,
    const int contentWidth,
    const MarkdownPalette& palette
) const {
    const auto& laid = m_document.laid();

    const auto drawScrollbar = [&](const int blockTopPx,
                                   const int blockHeight,
                                   const int blockContentWidth,
                                   const int blockNaturalWidth,
                                   const int scroll,
                                   const bool active) {
        if (blockNaturalWidth <= blockContentWidth) {
            return;
        }
        const int trackX = contentLeft;
        const int trackY = blockTopPx + blockHeight - kScrollbarHeight;
        const int trackW = contentWidth;
        const double ratio = static_cast<double>(blockContentWidth) / static_cast<double>(blockNaturalWidth);
        const int thumbW = std::max(kScrollbarMinThumb, static_cast<int>(trackW * ratio));
        const int maxScroll = blockNaturalWidth - blockContentWidth;
        const int travel = std::max(0, trackW - thumbW);
        const int thumbX = trackX + (maxScroll > 0 ? (scroll * travel / maxScroll) : 0);
        const unsigned char thumbAlpha = active ? kScrollbarThumbActiveAlpha : kScrollbarThumbAlpha;
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(wxColour(palette.text.Red(), palette.text.Green(), palette.text.Blue(), kScrollbarTrackAlpha)));
        gc.DrawRectangle(trackX, trackY, trackW, kScrollbarHeight);
        gc.SetBrush(wxBrush(wxColour(palette.text.Red(), palette.text.Green(), palette.text.Blue(), thumbAlpha)));
        gc.DrawRectangle(thumbX, trackY, thumbW, kScrollbarHeight);
    };
    for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
        const auto& block = laid.scrollBlocks.at(i);
        if (block.wrapped) {
            continue;
        }
        const int blockTopPx = contentTop + block.y;
        if (blockTopPx + block.height < update.y || blockTopPx > update.y + update.height) {
            continue;
        }
        const auto idx = static_cast<int>(i);
        const bool active = m_dragScrollBlockIndex == idx || m_hoverScrollBlockIndex == idx;
        drawScrollbar(blockTopPx, block.height, block.contentWidth, block.naturalWidth, m_blockScroll.at(i), active);
    }
}

auto MarkdownView::blockScrollOffset(const std::size_t index) const -> int {
    return index < m_blockScroll.size() ? m_blockScroll.at(index) : 0;
}

void MarkdownView::setBlockScrollOffset(const std::size_t index, const int offset) {
    if (index >= m_blockScroll.size()) {
        return;
    }
    const auto& block = m_document.laid().scrollBlocks.at(index);
    const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
    const int clamped = std::clamp(offset, 0, maxScroll);
    if (m_blockScroll.at(index) == clamped) {
        return;
    }
    m_blockScroll.at(index) = clamped;
    Refresh();
}

auto MarkdownView::scrollbarAt(const wxPoint& clientPoint) -> std::optional<ScrollbarTarget> {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int contentTopPx = m_contentPadding - originY;
    const int contentLeftPx = m_contentPadding;
    const int trackW = std::max(0, GetClientSize().GetWidth() - (2 * m_contentPadding));
    const auto& laid = m_document.laid();

    for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
        const auto& block = laid.scrollBlocks.at(i);
        if (block.wrapped || block.naturalWidth <= block.contentWidth) {
            continue;
        }
        // Edge-to-edge geometry — matches what `onPaint` draws.
        const int trackX = contentLeftPx;
        const int trackY = contentTopPx + block.y + block.height - kScrollbarHeight;
        if (clientPoint.y < trackY || clientPoint.y >= trackY + kScrollbarHeight) {
            continue;
        }
        if (clientPoint.x < trackX || clientPoint.x >= trackX + trackW) {
            continue;
        }
        const double ratio = static_cast<double>(block.contentWidth) / static_cast<double>(block.naturalWidth);
        const int thumbW = std::max(kScrollbarMinThumb, static_cast<int>(trackW * ratio));
        const int maxScroll = block.naturalWidth - block.contentWidth;
        const int travel = std::max(0, trackW - thumbW);
        const int thumbX = trackX + (maxScroll > 0 ? (m_blockScroll.at(i) * travel / maxScroll) : 0);
        return ScrollbarTarget {
            .blockIndex = i,
            .maxScroll = maxScroll,
            .trackX = trackX,
            .trackY = trackY,
            .trackW = trackW,
            .thumbX = thumbX,
            .thumbW = thumbW,
        };
    }
    return std::nullopt;
}

auto MarkdownView::linkAt(const wxPoint& clientPoint) const -> wxString {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int relX = clientPoint.x - m_contentPadding;
    const int relY = clientPoint.y + originY - m_contentPadding;
    const auto& laid = m_document.laid();
    for (const auto& line : laid.lines) {
        if (relY < line.y || relY >= line.y + line.height) {
            continue;
        }
        for (const auto& run : line.runs) {
            if (run.linkId < 0) {
                continue;
            }
            if (relX >= run.x && relX < run.x + run.width) {
                return laid.links.at(static_cast<std::size_t>(run.linkId)).url;
            }
        }
        break;
    }
    return {};
}

auto MarkdownView::hitTest(const wxPoint& clientPoint) -> SelectionPosition {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    int relX = clientPoint.x - m_contentPadding;
    const int relY = clientPoint.y + originY - m_contentPadding;
    const auto& laid = m_document.laid();
    if (laid.lines.empty()) {
        return {};
    }
    // Snap to the last line whose top is at-or-above the pointer. This
    // handles three cases cleanly:
    //   - pointer inside a line     → that line wins
    //   - pointer in a block gap    → the line above wins (no jump)
    //   - pointer past every line   → the last line wins
    //   - pointer above every line  → loop never assigns, default `0` stays
    std::size_t lineIdx = 0;
    for (std::size_t i = 0; i < laid.lines.size(); i++) {
        if (laid.lines.at(i).y <= relY) {
            lineIdx = i;
        } else {
            break;
        }
    }
    // Code / patch lines belonging to a non-wrapped block paint their
    // runs shifted by -scrollX; the visible click X maps back to a doc
    // x of `relX + scrollX` so per-run hit-testing finds the actual
    // character under the pointer.
    const auto& targetLine = laid.lines.at(lineIdx);
    if (targetLine.blockIndex >= 0
        && static_cast<std::size_t>(targetLine.blockIndex) < laid.scrollBlocks.size()) {
        const auto& block = laid.scrollBlocks.at(static_cast<std::size_t>(targetLine.blockIndex));
        if (!block.wrapped) {
            relX += m_blockScroll.at(static_cast<std::size_t>(targetLine.blockIndex));
        }
    }

    const wxClientDC clientDc(this);
    wxGCDC measureDc;
    measureDc.SetGraphicsContext(makeGraphicsContext(clientDc));
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);
    const auto [runIdx, charIdx] = hitTestLine(laid.lines.at(lineIdx), relX, measurer);
    return { .lineIndex = lineIdx, .runIndex = runIdx, .charInRun = charIdx };
}

void MarkdownView::clearSelection() {
    if (m_selection.empty()) {
        return;
    }
    m_selection.clear();
    Refresh();
}

void MarkdownView::copySelectionToClipboard() const {
    if (m_selection.empty()) {
        return;
    }
    const wxString text = extractSelectedText(m_document.laid(), m_selection);
    if (text.empty()) {
        return;
    }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(text));
        wxTheClipboard->Close();
    }
}

void MarkdownView::onMotion(wxMouseEvent& event) {
    if (m_dragScrollBlockIndex >= 0 && event.LeftIsDown()) {
        // Scrollbar-thumb drag — map the pointer's pixel travel back
        // into block-content coordinates so the thumb tracks the
        // mouse 1:1 while staying clamped to its valid range.
        const std::size_t idx = static_cast<std::size_t>(m_dragScrollBlockIndex);
        const auto& block = m_document.laid().scrollBlocks.at(idx);
        const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
        if (maxScroll > 0) {
            // Drag math must use the same panel-wide track width the
            // scrollbar paint uses — otherwise thumb travel scales by
            // the wrong amount.
            const int trackW = std::max(0, GetClientSize().GetWidth() - (2 * m_contentPadding));
            const double ratio = static_cast<double>(block.contentWidth) / static_cast<double>(block.naturalWidth);
            const int thumbW = std::max(kScrollbarMinThumb, static_cast<int>(trackW * ratio));
            const int travel = std::max(1, trackW - thumbW);
            const int deltaPx = event.GetPosition().x - m_dragScrollStartMouseX;
            const int newOffset = m_dragScrollStartOffset + (deltaPx * maxScroll / travel);
            setBlockScrollOffset(idx, newOffset);
        }
        event.Skip();
        return;
    }
    if (m_dragSelecting && event.LeftIsDown()) {
        m_selection.caret = hitTest(event.GetPosition());
        Refresh();
        event.Skip();
        return;
    }
    // Scrollbar hover — repaint when the highlighted thumb changes,
    // show a plain arrow over the track so the I-beam doesn't read as
    // text-selection there.
    const auto sbHit = scrollbarAt(event.GetPosition());
    const int prevHoverIdx = m_hoverScrollBlockIndex;
    m_hoverScrollBlockIndex = sbHit ? static_cast<int>(sbHit->blockIndex) : -1;
    if (prevHoverIdx != m_hoverScrollBlockIndex) {
        Refresh();
    }
    if (sbHit) {
        SetCursor(wxCursor(wxCURSOR_ARROW));
        event.Skip();
        return;
    }
    // Idle cursor: hand over links, I-beam over text content, arrow
    // otherwise. Cheap to recompute each motion — `linkAt` is a quick
    // scan and the line lookup is short.
    const bool overLink = !linkAt(event.GetPosition()).empty();
    if (overLink) {
        SetCursor(wxCursor(wxCURSOR_HAND));
    } else if (!m_selectable) {
        // Read-only: no I-beam, the content doesn't select.
        SetCursor(wxCursor(wxCURSOR_ARROW));
    } else {
        // I-beam when the point lands on a text-bearing line of the
        // laid document; arrow when it's in the empty area.
        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int relY = event.GetPosition().y + originY - m_contentPadding;
        const auto& laid = m_document.laid();
        const bool overText = std::ranges::any_of(
            laid.lines,
            [relY](const PaintLine& line) {
                return !line.runs.empty() && relY >= line.y && relY < line.y + line.height;
            }
        );
        SetCursor(overText ? wxCursor(wxCURSOR_IBEAM) : wxCursor(wxCURSOR_ARROW));
    }
    event.Skip();
}

void MarkdownView::onLeftDown(wxMouseEvent& event) {
    // Scrollbar-thumb / track click takes priority over both link and
    // selection clicks. A click on the track outside the thumb jumps
    // the thumb under the pointer first (single-segment "page jump")
    // and then enters drag mode so the user can continue moving.
    if (const auto hit = scrollbarAt(event.GetPosition())) {
        const int currentOffset = blockScrollOffset(hit->blockIndex);
        const bool onThumb = event.GetPosition().x >= hit->thumbX
                          && event.GetPosition().x < hit->thumbX + hit->thumbW;
        int dragStartOffset = currentOffset;
        if (!onThumb && hit->maxScroll > 0) {
            // Centre the thumb on the click point.
            const int travel = std::max(1, hit->trackW - hit->thumbW);
            const int newThumbX = std::clamp(event.GetPosition().x - (hit->thumbW / 2),
                hit->trackX,
                hit->trackX + travel);
            dragStartOffset = (newThumbX - hit->trackX) * hit->maxScroll / travel;
            setBlockScrollOffset(hit->blockIndex, dragStartOffset);
        }
        m_dragScrollBlockIndex = static_cast<int>(hit->blockIndex);
        m_dragScrollStartOffset = blockScrollOffset(hit->blockIndex);
        m_dragScrollStartMouseX = event.GetPosition().x;
        if (!HasCapture()) {
            CaptureMouse();
        }
        return;
    }

    // Link click takes priority over selection start — clicking on a
    // link shouldn't begin a drag-selection.
    const wxString url = linkAt(event.GetPosition());
    if (!url.empty()) {
        wxCommandEvent linkEvent(MARKDOWN_LINK_CLICKED, GetId());
        linkEvent.SetEventObject(this);
        linkEvent.SetString(url);
        if (!ProcessWindowEvent(linkEvent)) {
            wxLaunchDefaultBrowser(url);
        }
        return;
    }
    // Read-only mode — no selection, nothing else to do here.
    if (!m_selectable) {
        return;
    }
    // Start a new selection. Shift-click extends the existing one from
    // its anchor; a plain click sets anchor = caret = clicked position.
    const SelectionPosition pos = hitTest(event.GetPosition());
    if (event.ShiftDown() && !m_selection.empty()) {
        m_selection.caret = pos;
    } else {
        m_selection.anchor = pos;
        m_selection.caret = pos;
    }
    m_dragSelecting = true;
    if (!HasCapture()) {
        CaptureMouse();
    }
    SetFocus(); // ensure Ctrl+C / Ctrl+A reach us
    Refresh();
}

void MarkdownView::onLeftUp(wxMouseEvent& event) {
    if (m_dragScrollBlockIndex >= 0) {
        m_dragScrollBlockIndex = -1;
        if (HasCapture()) {
            ReleaseMouse();
        }
        event.Skip();
        return;
    }
    if (m_dragSelecting) {
        m_selection.caret = hitTest(event.GetPosition());
        m_dragSelecting = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
        Refresh();
    }
    event.Skip();
}

void MarkdownView::onLeftDClick(wxMouseEvent& event) {
    if (!m_selectable) {
        event.Skip();
        return;
    }
    // Double-click selects the word under the cursor: walk back from
    // the hit position to the nearest whitespace, walk forward to the
    // next, and snap the selection between them.
    const SelectionPosition pos = hitTest(event.GetPosition());
    const auto& laid = m_document.laid();
    if (pos.lineIndex >= laid.lines.size()) {
        event.Skip();
        return;
    }
    const auto& line = laid.lines.at(pos.lineIndex);
    if (pos.runIndex >= line.runs.size()) {
        event.Skip();
        return;
    }
    const auto& run = line.runs.at(pos.runIndex);
    const auto isWord = [](const wxUniChar ch) {
        return wxIsalnum(ch) || ch == '_';
    };
    std::size_t start = pos.charInRun;
    while (start > 0 && isWord(run.text.GetChar(start - 1))) {
        start--;
    }
    std::size_t end = pos.charInRun;
    while (end < run.text.length() && isWord(run.text.GetChar(end))) {
        end++;
    }
    m_selection.anchor = { .lineIndex = pos.lineIndex, .runIndex = pos.runIndex, .charInRun = start };
    m_selection.caret = { .lineIndex = pos.lineIndex, .runIndex = pos.runIndex, .charInRun = end };
    Refresh();
}

void MarkdownView::onCharHook(wxKeyEvent& event) {
    if (m_selectable && (event.ControlDown() || event.CmdDown())) {
        if (event.GetKeyCode() == 'C') {
            copySelectionToClipboard();
            return;
        }
        if (event.GetKeyCode() == 'A') {
            const auto& laid = m_document.laid();
            if (!laid.lines.empty()) {
                const std::size_t lastLine = laid.lines.size() - 1;
                const auto& last = laid.lines.at(lastLine);
                m_selection.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
                const std::size_t lastRun = last.runs.empty() ? 0 : last.runs.size() - 1;
                const std::size_t lastChar = last.runs.empty() ? 0 : last.runs.at(lastRun).text.length();
                m_selection.caret = { .lineIndex = lastLine, .runIndex = lastRun, .charInRun = lastChar };
                Refresh();
            }
            return;
        }
    }
    if (event.GetKeyCode() == WXK_ESCAPE) {
        clearSelection();
        return;
    }
    event.Skip();
}

void MarkdownView::onMouseWheel(wxMouseEvent& event) {
    // Locate the overflowing scroll block under the pointer, if any.
    // Used by horizontal trackpad swipes and by shift+wheel to route
    // horizontal scroll to the right block.
    const auto findOverflowingAt = [&](const int docY) -> int {
        const auto& laid = m_document.laid();
        for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
            const auto& block = laid.scrollBlocks.at(i);
            if (block.wrapped || block.naturalWidth <= block.contentWidth) {
                continue;
            }
            if (docY < block.y || docY >= block.y + block.height) {
                continue;
            }
            return static_cast<int>(i);
        }
        return -1;
    };

    // Horizontal trackpad swipe — route to the overflowing block under
    // the pointer with a fractional accumulator so fine movements don't
    // get rounded away.
    if (event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL) {
        const int wheelDelta = event.GetWheelDelta();
        if (wheelDelta <= 0) {
            event.Skip();
            return;
        }
        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int docY = event.GetPosition().y + originY - m_contentPadding;
        const int idx = findOverflowingAt(docY);
        if (idx < 0) {
            event.Skip();
            return;
        }
        constexpr int kDamperNum = 9;
        constexpr int kDamperDen = 10;
        const int divisor = wheelDelta * kDamperDen;
        m_hwheelPixelAccum += event.GetWheelRotation() * kHorizPxPerNotch * kDamperNum;
        const int pixels = m_hwheelPixelAccum / divisor;
        m_hwheelPixelAccum -= pixels * divisor;
        if (pixels != 0) {
            setBlockScrollOffset(static_cast<std::size_t>(idx), m_blockScroll.at(static_cast<std::size_t>(idx)) + pixels);
        }
        return;
    }
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        event.Skip();
        return;
    }
    // Shift + wheel inside an overflowing code/patch block scrolls the
    // block horizontally rather than scrolling the conversation.
    // Plain wheel keeps its vertical-scroll behaviour even over a
    // block so a long doc stays browsable.
    if (event.ShiftDown()) {
        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int docY = event.GetPosition().y + originY - m_contentPadding;
        const int idx = findOverflowingAt(docY);
        if (idx >= 0) {
            const int rotation = event.GetWheelRotation();
            const int delta = (rotation > 0 ? -1 : 1) * kScrollbarWheelStep;
            setBlockScrollOffset(static_cast<std::size_t>(idx), m_blockScroll.at(static_cast<std::size_t>(idx)) + delta);
            return;
        }
        // Not over an overflowing block — fall through to vertical scroll.
    }
    // Same rotation-to-pixels translation as `AiChatView` — one wheel
    // notch ≈ 3 body lines, with a fractional accumulator so macOS
    // trackpad momentum tails aren't rounded away.
    const int wheelDelta = event.GetWheelDelta();
    if (wheelDelta <= 0) {
        event.Skip();
        return;
    }
    constexpr int kDamperNum = 9;
    constexpr int kDamperDen = 10;
    const int pxPerNotch = std::max(24, m_bodyLineHeight * 3);
    const int divisor = wheelDelta * kDamperDen;
    m_wheelPixelAccum += event.GetWheelRotation() * pxPerNotch * kDamperNum;
    const int pixels = m_wheelPixelAccum / divisor;
    m_wheelPixelAccum -= pixels * divisor;
    if (pixels == 0) {
        return;
    }
    int viewX = 0;
    int viewY = 0;
    GetViewStart(&viewX, &viewY);
    Scroll(viewX, std::max(0, viewY - pixels));
}
