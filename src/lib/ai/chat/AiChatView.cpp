//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatView.hpp"
#include "CodeActionBar.hpp"
#include "CodeHighlighter.hpp"
#include "ai/AiManager.hpp"
#include "ai/Patch.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "markdown/Markdown.hpp"
#include "markdown/MarkdownLayout.hpp"
using namespace fbide;
using namespace fbide::ai;
using namespace fbide::markdown;

namespace {

/// Build a graphics context using the Direct2D renderer when available
/// (Windows 10+/8.1, or 7 with the D2D Platform Update), falling back to
/// the platform default otherwise. Direct2D + DirectWrite is required for
/// colour emoji on Windows — GDI+ only produces monochrome glyphs from
/// Segoe UI Symbol and silently drops most pictographs.
///
/// IMPORTANT: D2D batches draw commands until `Flush()` or the context is
/// destroyed. If you draw via a `wxGCDC` and then immediately `Blit` from
/// the backing memory DC, you must call
/// `wxGCDC::GetGraphicsContext()->Flush()` first — otherwise the blit
/// reads the bitmap before D2D writes have landed, producing black flashes
/// and ghosted content.
template<typename DcT>
auto makeChatGraphicsContext(DcT& target) -> wxGraphicsContext* {
    wxGraphicsRenderer* renderer = nullptr;
#ifdef __WXMSW__
    renderer = wxGraphicsRenderer::GetDirect2DRenderer();
#endif
    if (renderer == nullptr) {
        renderer = wxGraphicsRenderer::GetDefaultRenderer();
    }
    return renderer->CreateContext(target);
}

// Outer margin between the view edge and the bubbles.
constexpr int kMargin = 12;
// Vertical gap between consecutive message bubbles.
constexpr int kMessageGap = 10;
// Padding between a bubble's edge and its content.
constexpr int kBubblePad = 10;
// Corner radius of a message bubble.
constexpr int kBubbleRadius = 10;
// Largest fraction of the available width a bubble may occupy.
constexpr double kBubbleMaxFraction = 0.95;
// Smallest content width a bubble shrinks to.
constexpr int kMinBubbleContent = 60;
// Inset of the action bar from the code block's top-right corner.
constexpr int kActionBarInset = 4;

/// True when `lang` (a fence tag) denotes FreeBASIC. Requires an explicit
/// tag — an untagged ```...``` fence is NOT assumed to be FreeBASIC.
/// Untagged blocks rendered as model output are typically shell commands,
/// pseudo-code, or generic snippets that the FB lexer would mangle if it
/// tried to colour them.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
}

/// Stable UTF-8 key for a parsed SEARCH/REPLACE block — used by the
/// "already applied" set so live-edit and the manual apply path agree
/// on which proposals have been handled. Marker separator is anything
/// that can't appear in real source-file text.
auto patchKey(const LaidScrollBlock& patch) -> std::string {
    return (patch.patchSearch + wxString("\n>>>\n") + patch.patchReplace).utf8_string();
}

/// True when `url` uses a scheme we are willing to hand to the OS.
/// `wxLaunchDefaultBrowser` passes the URL to the platform handler,
/// which would happily honour `file://`, `vbscript:`, etc. for a model
/// reply that embeds a hostile link. Whitelist the safe schemes.
auto isSafeLinkUrl(const wxString& url) -> bool {
    const wxString lower = url.Lower();
    return lower.StartsWith("http://")
        || lower.StartsWith("https://")
        || lower.StartsWith("mailto:");
}

/// Linear blend of two colours — `t` of 0 yields `a`, 1 yields `b`.
auto blend(const wxColour& a, const wxColour& b, const double t) -> wxColour {
    const auto mix = [t](const unsigned char from, const unsigned char to) {
        return static_cast<unsigned char>(from + ((to - from) * t));
    };
    return { mix(a.Red(), b.Red()), mix(a.Green(), b.Green()), mix(a.Blue(), b.Blue()) };
}

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(AiChatView, wxScrolled)
    EVT_PAINT(AiChatView::onPaint)
    EVT_SIZE(AiChatView::onSize)
    EVT_MOTION(AiChatView::onMotion)
    EVT_LEFT_DOWN(AiChatView::onLeftDown)
    EVT_LEFT_UP(AiChatView::onLeftUp)
    EVT_LEFT_DCLICK(AiChatView::onLeftDClick)
    EVT_CHAR_HOOK(AiChatView::onCharHook)
    EVT_LEAVE_WINDOW(AiChatView::onLeaveWindow)
    EVT_SCROLLWIN(AiChatView::onScroll)
    EVT_MOUSEWHEEL(AiChatView::onMouseWheel)
    EVT_BUTTON(ID_CodeCopy, AiChatView::onCopyCode)
    EVT_BUTTON(ID_CodeInsert, AiChatView::onInsertCode)
    EVT_BUTTON(ID_CodeRun, AiChatView::onRunCode)
    EVT_BUTTON(ID_PatchApply, AiChatView::onApplyPatch)
    EVT_BUTTON(ID_PatchReject, AiChatView::onRejectPatch)
    EVT_COMMAND(wxID_ANY, EVT_CODE_BAR_LEAVE, AiChatView::onBarLeave)
wxEND_EVENT_TABLE()
// clang-format on

AiChatView::AiChatView(wxWindow* parent, Context& ctx)
: wxScrolled(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_ctx(ctx) {
    wxScrolled::SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Pixel-granular scroll so high-rate input devices (trackpads, smooth
    // wheels) don't snap to a coarser step.
    SetScrollRate(0, 1);

    resolveFonts();
    rebuildBubbleBrushes();
    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);
    m_imageCache = std::make_unique<MarkdownImageCache>();
    // A finished download invalidates the cached layout (the image now
    // has real dimensions to lay out around). Coalesce multiple
    // notifications that settle in the same event-loop tick — when a
    // reply embeds several images, they often resolve almost together.
    m_imageCache->setListener([this](const wxString& /*url*/) {
        if (m_imageRelayoutPending) {
            return;
        }
        m_imageRelayoutPending = true;
        CallAfter([this] {
            m_imageRelayoutPending = false;
            relayout();
            Refresh();
        });
    });

    // One reusable action bar, shown over whichever code block is hovered.
    // It is a child of this scroll surface, so it scrolls with the content.
    m_actionBar = make_unowned<CodeActionBar>(this, m_ctx);
    m_actionBar->Hide();
}

AiChatView::~AiChatView() = default;

void AiChatView::resolveFonts() {
    // Body starts from the platform's default GUI font. `[ai] fontSize`
    // overrides the point size when set (>0); leaving it unset / empty
    // keeps the system default. The teletype and theme fonts then
    // follow the body size so the three faces line up vertically in
    // the chat bubble (a theme font like 14pt JetBrains Mono inside a
    // 13pt prose bubble looked off before this).
    // Body / mono / themed fonts are about to be replaced — every cache
    // entry holds a wxFont derived from the old set and would now lie.
    m_measurerCache.clear();

    m_bodyFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    const auto fontSize = m_ctx.getConfigManager().config().at("ai.fontSize").as<int>();
    if (fontSize.has_value() && *fontSize > 0) {
        m_bodyFont.SetPointSize(*fontSize);
    }
    const int size = m_bodyFont.GetPointSize();
    m_monoFont = wxFont(wxFontInfo(size).Family(wxFONTFAMILY_TELETYPE));
    m_themedFont = m_ctx.getTheme().getResolvedFont();
    m_themedFont.SetPointSize(size);

    // Body line-height drives the per-notch wheel scroll amount in
    // onMouseWheel — matches the leading used by DcMeasurer so a
    // wheel notch scrolls a round number of visible lines.
    wxClientDC dc(this);
    wxCoord textWidth = 0;
    wxCoord textHeight = 0;
    dc.GetTextExtent("Ag", &textWidth, &textHeight, nullptr, nullptr, &m_bodyFont);
    m_bodyLineHeight = textHeight + 4;
}

void AiChatView::setMessages(std::vector<ChatViewMessage> messages) {
    m_messages = std::move(messages);
    // A reflow may invalidate the selection's (line, run, char) positions
    // — easier to drop the selection than to try to remap it.
    m_selectionMessage = -1;
    m_selection.clear();
    relayout();               // per-message caching re-lays only what actually changed
    Scroll(0, m_totalHeight); // keep pinned to the newest — wxScrolled clamps to the max
    Refresh();

    if (m_ctx.getAiManager().isLiveEdit()) {
        autoApplyPatches();
    }
}

void AiChatView::refreshTheme() {
    // Keyword groups may have changed — rebuild the configured lexer.
    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);
    resolveFonts();
    rebuildBubbleBrushes();
    m_layoutWidth = -1;
    hideActionBar();
    relayout();
    Refresh();
}

void AiChatView::rebuildBubbleBrushes() {
    m_userBubbleBrush = wxBrush(bubbleColour(true));
    m_assistantBubbleBrush = wxBrush(bubbleColour(false));
}

void AiChatView::onSize(wxSizeEvent& event) {
    hideActionBar(); // bubble positions shift — re-hover brings the bar back
    // A width change re-wraps every bubble's content. Convert the
    // current selection to flat character offsets (stable across
    // re-wrap), relayout, then convert back — the highlight stays
    // anchored on the same characters inside the same bubble.
    const bool widthChanged = GetClientSize().GetWidth() != m_layoutWidth;
    const bool remap = widthChanged && m_selectionMessage >= 0 && !m_selection.empty();
    std::size_t anchorOffset = 0;
    std::size_t caretOffset = 0;
    if (remap) {
        const auto& laid = m_items.at(static_cast<std::size_t>(m_selectionMessage)).document.laid();
        anchorOffset = selectionToOffset(laid, m_selection.anchor);
        caretOffset = selectionToOffset(laid, m_selection.caret);
    }
    relayout();
    if (remap) {
        const auto& laid = m_items.at(static_cast<std::size_t>(m_selectionMessage)).document.laid();
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

void AiChatView::relayout() {
    const int panelWidth = GetClientSize().GetWidth();

    wxClientDC clientDc(this);
    wxGCDC measureDc;
    measureDc.SetGraphicsContext(makeChatGraphicsContext(clientDc));
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);

    const MarkdownPalette pal = palette();

    // `markdown.wrapCodeBlocks` (true by default) controls whether code
    // and patch lines soft-wrap or scroll horizontally. Read once per
    // relayout — config is cheap, no need to cache it elsewhere.
    const auto wrapOpt = m_ctx.getConfigManager().config().at("markdown.wrapCodeBlocks").as<bool>();
    const bool wrapCodeBlocks = wrapOpt.value_or(true);
    m_wrapCodeBlocks = wrapCodeBlocks;

    // A bubble may take at most kBubbleMaxFraction of the inter-margin width,
    // leaving a gutter on the opposite side.
    const int available = std::max(120, panelWidth - (2 * kMargin));
    const int maxBubble = std::max(100, static_cast<int>(available * kBubbleMaxFraction));
    const int maxContent = std::max(kMinBubbleContent, maxBubble - (2 * kBubblePad));

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

    std::vector<LaidMessage> rebuilt;
    rebuilt.reserve(m_messages.size());
    int y = kMargin;
    for (std::size_t index = 0; index < m_messages.size(); index++) {
        const auto& message = m_messages[index];
        LaidMessage item;
        item.fromUser = message.fromUser;

        // Move the previous document for this slot if the role matches —
        // its (markdown, width) cache makes `setMarkdown` a no-op when
        // neither has changed (the common case while a reply streams:
        // only the last bubble's text actually moves). Per-block scroll
        // state migrates with the document so a resize doesn't reset
        // the user's horizontal scroll position.
        if (index < m_items.size() && m_items[index].fromUser == message.fromUser) {
            item.document = std::move(m_items[index].document);
            item.contentWidth = m_items[index].contentWidth;
            item.blockScroll = std::move(m_items[index].blockScroll);
        }

        // Reformat model replies; leave the user's own code untouched.
        const bool reformat = !message.fromUser;
        const auto highlight = [this, reformat](const wxString& code, const wxString& lang) {
            return highlightFence(code, lang, reformat);
        };
        const bool documentRebuilt = item.document.setMarkdown(
            message.markdown, maxContent, measurer, pal, highlight, resolveImage, wrapCodeBlocks
        );
        if (documentRebuilt) {
            // Shrink the bubble to its widest line — wrapping was done
            // at maxContent, so every line already fits the shrunk width.
            // Non-wrapped code / patch lines pay an extra right gutter
            // (`kBlockTextGutter`) so the longest line doesn't touch
            // the bubble edge; wrap-mode lines already break at the
            // bubble's right gutter, so they need no extra room.
            constexpr int kBlockTextGutter = 8; // mirrors layout's kCodePadding
            int widest = 0;
            const auto& laid = item.document.laid();
            for (const auto& line : laid.lines) {
                int lineRight = 0;
                for (const auto& run : line.runs) {
                    lineRight = std::max(lineRight, run.x + run.width);
                }
                if (lineRight > 0 && line.blockIndex >= 0) {
                    const auto idx = static_cast<std::size_t>(line.blockIndex);
                    if (idx < laid.scrollBlocks.size() && !laid.scrollBlocks.at(idx).wrapped) {
                        lineRight += kBlockTextGutter;
                    }
                }
                widest = std::max(widest, lineRight);
            }
            item.contentWidth = std::clamp(widest, kMinBubbleContent, maxContent);
        }

        // Sync the per-block scroll vector to the laid doc's block count,
        // preserving any pre-existing offsets and clamping to the new
        // overflow range.
        const auto& laid = item.document.laid();
        item.blockScroll.resize(laid.scrollBlocks.size(), 0);
        for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
            const auto& block = laid.scrollBlocks.at(i);
            const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
            item.blockScroll[i] = std::clamp(item.blockScroll[i], 0, maxScroll);
        }

        const int bubbleWidth = item.contentWidth + (2 * kBubblePad);
        const int bubbleHeight = item.document.height() + (2 * kBubblePad);
        const int bubbleX = message.fromUser
                              ? (panelWidth - kMargin - bubbleWidth)
                              : kMargin;
        item.bubble = wxRect(bubbleX, y, bubbleWidth, bubbleHeight);

        y += bubbleHeight + kMessageGap;
        rebuilt.push_back(std::move(item));
    }

    m_items = std::move(rebuilt);
    m_totalHeight = m_items.empty() ? 0 : (y - kMessageGap + kMargin);
    m_layoutWidth = panelWidth;
    SetVirtualSize(panelWidth, m_totalHeight);
}

void AiChatView::onPaint(wxPaintEvent& /*event*/) {
    wxPaintDC paintDc(this);
    const wxSize size = GetClientSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0) {
        return;
    }

    // Reuse a single off-screen buffer across paints; reallocate only when
    // the window resizes OR the DPI changes (drag the window to / from a
    // Retina display). The bitmap is allocated at PHYSICAL pixels with
    // its DIP scale set so wxGCDC rasterises text at the screen's native
    // resolution — without this, fonts and emoji render at 1× and get
    // bilinear-upscaled on the blit, looking fuzzy on Retina.
    const double scale = GetDPIScaleFactor();
    if (!m_buffer.IsOk()
        || m_buffer.GetDIPSize() != size
        || m_buffer.GetScaleFactor() != scale) {
        m_buffer.CreateWithDIPSize(size, scale);
    }

    const wxRect update = GetUpdateRegion().GetBox();

    wxMemoryDC memoryDc(m_buffer);
    {
        wxGCDC gc;
        gc.SetGraphicsContext(makeChatGraphicsContext(memoryDc));

        // Repaint only the update rect — fill the band with background, draw
        // through it, blit it. Pixels outside the rect stay as they were.
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
        gc.DrawRectangle(update);

        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int regionTopDoc = originY + update.y;
        const int regionBottomDoc = regionTopDoc + update.height;

        // Resolve palette once per paint — every visible bubble shares it.
        const MarkdownPalette pal = palette();

        // Bubbles are stacked in document order — binary-search to the first one
        // that can intersect the dirty band rather than scanning the whole list.
        const auto first = std::lower_bound(
            m_items.begin(), m_items.end(), regionTopDoc,
            [](const LaidMessage& message, const int top) { return message.bubble.GetBottom() < top; }
        );
        // One measurer reused across every bubble. The cache lives on
        // the view (`m_measurerCache`) so font + width lookups already
        // populated by `relayout` are reused for selection-highlight
        // partial-run measurement.
        const DcMeasurer measurer(memoryDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);

        for (auto it = first; it != m_items.end(); ++it) {
            if (it->bubble.GetTop() > regionBottomDoc) {
                break; // first bubble past the band — the rest are too
            }
            const auto idx = static_cast<std::size_t>(std::distance(m_items.begin(), it));
            paintMessage(gc, *it, idx, pal, measurer, originY, update.y, update.y + update.height);
        }

        // Force any pending draw commands to land on the bitmap before the
        // blit below reads it. Direct2D batches commands until EndDraw —
        // without this flush the blit picks up stale pixels and the bubble
        // flashes black / leaves ghost rows during scroll.
        gc.GetGraphicsContext()->Flush();
    } // gc out of scope → wxMemoryDC pixels now safe to blit

    paintDc.Blit(update.x, update.y, update.width, update.height, &memoryDc, update.x, update.y);
}

void AiChatView::paintMessage(
    wxGCDC& gc,
    const LaidMessage& message,
    const std::size_t messageIndex,
    const MarkdownPalette& pal,
    const TextMeasurer& measurer,
    const int originY,
    const int updateTop,
    const int updateBottom
) const {
    // Bubble — a rounded rect. Content stays within the rect by layout, so
    // no per-message clipping region is needed.
    wxRect bubble = message.bubble;
    bubble.y -= originY;
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(message.fromUser ? m_userBubbleBrush : m_assistantBubbleBrush);
    gc.DrawRoundedRectangle(bubble, kBubbleRadius);

    const int contentLeft = bubble.x + kBubblePad;
    const int contentTop = bubble.y + kBubblePad;

    // Binary-search to the first line that can be inside the dirty band.
    // Lines are stacked by y in the laid-out document.
    const auto& laid = message.document.laid();
    const int updateTopRel = updateTop - contentTop;
    const int updateBottomRel = updateBottom - contentTop;
    auto first = std::lower_bound(
        laid.lines.begin(), laid.lines.end(), updateTopRel,
        [](const PaintLine& line, const int top) { return line.y + line.height < top; }
    );

    // Shared DC-state cache — adjacent runs and lines mostly share style /
    // colour, so reusing the cache across the whole loop avoids redundant
    // SetFont / SetTextForeground / GetFontMetrics calls.
    PaintRunState runState;

    // Selection lives on one bubble at a time — only this bubble paints
    // the highlight. The measurer is hoisted from `onPaint` so it's
    // shared across bubbles for cheap partial-run width measurement.
    const bool hasSelection = (m_selectionMessage >= 0)
                           && (std::cmp_equal(m_selectionMessage, messageIndex))
                           && !m_selection.empty();
    // Translucent selection so code / patch / table backgrounds and
    // inline images blend through — solid would obscure them and the
    // band would read as an opaque blue strip.
    const wxColour sysHighlight = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    constexpr unsigned char kHighlightAlpha = 100;
    const wxColour highlightColour(sysHighlight.Red(), sysHighlight.Green(), sysHighlight.Blue(), kHighlightAlpha);

    for (auto it = first; it != laid.lines.end(); ++it) {
        const auto& line = *it;
        if (line.y > updateBottomRel) {
            break;
        }
        const auto lineIdx = static_cast<std::size_t>(std::distance(laid.lines.begin(), it));
        const int lineTop = contentTop + line.y;
        const auto next = std::next(it);
        const int nextLineY = (next == laid.lines.end()) ? -1 : next->y;
        markdown::paintLineBackground(gc, line, contentLeft, lineTop, message.contentWidth, pal);

        // Code / patch lines that belong to a non-wrapped block paint
        // their runs shifted by the block's horizontal scroll offset
        // and clipped to the block's content rect — the bubble-wide
        // background stays put.
        int scrollX = 0;
        bool clipped = false;
        if (line.blockIndex >= 0
            && static_cast<std::size_t>(line.blockIndex) < laid.scrollBlocks.size()) {
            const auto& block = laid.scrollBlocks.at(static_cast<std::size_t>(line.blockIndex));
            if (!block.wrapped) {
                scrollX = message.blockScroll.at(static_cast<std::size_t>(line.blockIndex));
                gc.SetClippingRegion(contentLeft + block.contentLeft, lineTop, block.contentWidth, line.height);
                clipped = true;
            }
        }

        if (hasSelection) {
            markdown::paintSelectionHighlight(gc, line, lineIdx, contentLeft - scrollX, lineTop, message.contentWidth, nextLineY, m_selection, highlightColour, measurer);
        }
        markdown::paintLineText(gc, line, contentLeft - scrollX, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);

        if (clipped) {
            gc.DestroyClippingRegion();
        }
    }

    // Scrollbar pass — one thin track + thumb per overflowing
    // non-wrapped code / patch block, sitting on top of the block's
    // bottom padding strip. The track stretches edge-to-edge across
    // the bubble's content rect (independent of the block's text-only
    // inner padding); the thumb's size and position still derive from
    // block content vs. natural width. Skipped entirely when wrap is on.
    constexpr int kScrollbarHeight = 6;
    constexpr int kScrollbarMinThumb = 24;
    constexpr unsigned char kScrollbarTrackAlpha = 60;
    constexpr unsigned char kScrollbarThumbAlpha = 160;
    constexpr unsigned char kScrollbarThumbActiveAlpha = 220;
    const bool dragActive = std::cmp_equal(m_dragScrollMessageIndex, messageIndex);
    const bool hoverActive = std::cmp_equal(m_hoverScrollMessageIndex, messageIndex);
    const auto drawScrollbar = [&](const int blockY,
                                   const int blockHeight,
                                   const int blockContentWidth,
                                   const int blockNaturalWidth,
                                   const int scroll,
                                   const bool active) {
        if (blockNaturalWidth <= blockContentWidth) {
            return;
        }
        const int trackX = contentLeft;
        const int trackY = contentTop + blockY + blockHeight - kScrollbarHeight;
        const int trackW = message.contentWidth;
        const double ratio = static_cast<double>(blockContentWidth) / static_cast<double>(blockNaturalWidth);
        const int thumbW = std::max(kScrollbarMinThumb, static_cast<int>(trackW * ratio));
        const int maxScroll = blockNaturalWidth - blockContentWidth;
        const int travel = std::max(0, trackW - thumbW);
        const int thumbX = trackX + (maxScroll > 0 ? (scroll * travel / maxScroll) : 0);
        const unsigned char thumbAlpha = active ? kScrollbarThumbActiveAlpha : kScrollbarThumbAlpha;
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(wxColour(pal.text.Red(), pal.text.Green(), pal.text.Blue(), kScrollbarTrackAlpha)));
        gc.DrawRectangle(trackX, trackY, trackW, kScrollbarHeight);
        gc.SetBrush(wxBrush(wxColour(pal.text.Red(), pal.text.Green(), pal.text.Blue(), thumbAlpha)));
        gc.DrawRectangle(thumbX, trackY, thumbW, kScrollbarHeight);
    };
    for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
        const auto& block = laid.scrollBlocks.at(i);
        if (block.wrapped) {
            continue;
        }
        const bool active = (dragActive && m_dragScrollBlockIndex == i)
                         || (hoverActive && m_hoverScrollBlockIndex == i);
        drawScrollbar(block.y, block.height, block.contentWidth, block.naturalWidth, message.blockScroll.at(i), active);
    }

    // Overlay applied SEARCH/REPLACE proposals with a translucent veil so
    // the chat thread distinguishes resolved cards from still-actionable
    // ones at a glance. Drawn after content so it dims (rather than
    // hides) the underlying strips and text.
    if (!m_appliedPatches.empty()) {
        const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        const wxColour overlay(windowText.Red(), windowText.Green(), windowText.Blue(), 90);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(overlay));
        for (const auto& block : laid.scrollBlocks) {
            if (block.kind != LaidScrollBlock::Kind::Patch
                || !m_appliedPatches.contains(patchKey(block))) {
                continue;
            }
            const int patchY = contentTop + block.y;
            if (patchY + block.height < updateTop || patchY > updateBottom) {
                continue;
            }
            gc.DrawRectangle(contentLeft, patchY, message.contentWidth, block.height);
        }
    }
}

auto AiChatView::bubbleColour(const bool fromUser) const -> wxColour {
    const wxColour windowBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    if (fromUser) {
        // Accent-tinted toward the system highlight colour.
        return blend(windowBg, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), 0.20);
    }
    // A subtle surface, nudged toward the text colour.
    return blend(windowBg, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 0.07);
}

auto AiChatView::palette() const -> MarkdownPalette {
    const auto& theme = m_ctx.getTheme();
    const wxColour separator = theme.getSeparator();
    const wxColour windowBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    return {
        .text = windowText,
        .link = wxColour(40, 100, 220),
        .codeBg = theme.background({}),
        .inlineCodeBg = theme.background({}),
        .rule = separator.IsOk() ? separator : wxColour(180, 180, 180),
        // Header tint is derived from system colours (not the editor
        // theme) so it always contrasts with `text`. The editor theme
        // can be dark while the OS is in light mode (and vice versa);
        // using `codeBg` here makes the header invisible in those mixes.
        .tableHeaderBg = blend(windowBg, windowText, 0.14),
        // SEARCH / REPLACE tints — blended into the editor code background
        // so they sit on the same surface as fenced code, but pushed
        // toward the theme's diff palette (Removed / Added) so a glance
        // distinguishes them. Sourcing from Theme lets a user retune the
        // patch card colour by editing one place.
        .patchSearchBg = blend(theme.background({}), theme.getChangesRemoved(), 0.30),
        .patchReplaceBg = blend(theme.background({}), theme.getChangesAdded(), 0.30),
        .patchFg = theme.getChangesForeground(),
    };
}

auto AiChatView::highlightFence(const wxString& code, const wxString& lang, const bool reformat) const
    -> std::vector<CodeLine> {
    if (isFreeBasicTag(lang)) {
        return m_highlighter->highlight(code, reformat);
    }

    // Non-FreeBASIC fence — render as plain, default-coloured lines.
    const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    std::vector<CodeLine> lines;
    CodeLine current;
    wxString segment;
    for (const wxUniChar ch : code) {
        if (ch == '\n') {
            if (!segment.empty()) {
                current.push_back({ .text = segment, .colour = foreground });
            }
            lines.push_back(current);
            current.clear();
            segment.clear();
        } else {
            segment += ch;
        }
    }
    if (!segment.empty()) {
        current.push_back({ .text = segment, .colour = foreground });
    }
    lines.push_back(current);
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

auto AiChatView::linkAt(const wxPoint& clientPoint) const -> wxString {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);

    for (const auto& item : m_items) {
        if (!item.bubble.Contains(docPoint)) {
            continue;
        }
        const int contentLeft = item.bubble.x + kBubblePad;
        const int contentTop = item.bubble.y + kBubblePad;
        for (const auto& line : item.document.laid().lines) {
            for (const auto& run : line.runs) {
                if (run.linkId < 0) {
                    continue;
                }
                const wxRect runRect(contentLeft + run.x, contentTop + line.y, run.width, line.height);
                if (runRect.Contains(docPoint)) {
                    return item.document.laid().links[static_cast<std::size_t>(run.linkId)].url;
                }
            }
        }
        break; // only one bubble can contain the point
    }
    return {};
}

auto AiChatView::codeBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int> {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);

    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        const auto& item = m_items[mi];
        if (!item.bubble.Contains(docPoint)) {
            continue;
        }
        const int contentLeft = item.bubble.x + kBubblePad;
        const int contentTop = item.bubble.y + kBubblePad;
        const auto& blocks = item.document.laid().scrollBlocks;
        for (std::size_t bi = 0; bi < blocks.size(); bi++) {
            if (blocks[bi].kind != LaidScrollBlock::Kind::Code) {
                continue;
            }
            const auto& block = blocks[bi];
            const wxRect codeRect(contentLeft, contentTop + block.y, item.contentWidth, block.height);
            if (codeRect.Contains(docPoint)) {
                return { static_cast<int>(mi), static_cast<int>(bi) };
            }
        }
        break;
    }
    return { -1, -1 };
}

auto AiChatView::patchBlockAt(const wxPoint& clientPoint) const -> std::pair<int, int> {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);

    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        const auto& item = m_items[mi];
        if (!item.bubble.Contains(docPoint)) {
            continue;
        }
        const int contentLeft = item.bubble.x + kBubblePad;
        const int contentTop = item.bubble.y + kBubblePad;
        const auto& blocks = item.document.laid().scrollBlocks;
        for (std::size_t bi = 0; bi < blocks.size(); bi++) {
            if (blocks[bi].kind != LaidScrollBlock::Kind::Patch) {
                continue;
            }
            const auto& block = blocks[bi];
            const wxRect patchRect(contentLeft, contentTop + block.y, item.contentWidth, block.height);
            if (patchRect.Contains(docPoint)) {
                return { static_cast<int>(mi), static_cast<int>(bi) };
            }
        }
        break;
    }
    return { -1, -1 };
}

void AiChatView::showActionBar(const int messageIndex, const int blockIndex, const CodeActionBar::Mode mode) {
    if (messageIndex < 0 || blockIndex < 0) {
        hideActionBar();
        return;
    }
    m_barMessage = messageIndex;
    m_barIndex = blockIndex;
    m_actionBar->setMode(mode);

    const auto& item = m_items[static_cast<std::size_t>(messageIndex)];
    // `blockIndex` indexes into the unified scrollBlocks vector — the
    // bar's mode (CodeSample / PatchProposal) is set by the caller from
    // the block's kind, but the geometry comes from the same field set
    // either way.
    const auto& block = item.document.laid().scrollBlocks[static_cast<std::size_t>(blockIndex)];
    const int blockY = block.y;

    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int codeRight = item.bubble.x + kBubblePad + item.contentWidth;
    const int codeTopClient = item.bubble.y + kBubblePad + blockY - originY;

    const wxSize barSize = m_actionBar->GetSize();
    const int xClient = codeRight - barSize.GetWidth() - kActionBarInset;

    // Attached when the snippet's top edge is inside the visible area; the
    // bar tracks the code block and scrolls with the content. Detached when
    // the top has scrolled above the viewport — pin the bar just below the
    // scroll surface's own top so it stays visible while the user scrolls
    // through a long snippet.
    const bool attached = codeTopClient >= 0;

    int x = 0;
    int y = 0;
    if (attached) {
        // Position tracks the snippet in scroll-surface client coordinates.
        x = xClient;
        y = codeTopClient + kActionBarInset;
    } else {
        // Pin to the top of the visible area. ScreenToClient/ClientToScreen
        // is identity here — kept explicit to mark this as a coordinate
        // translation, not a stray offset.
        const wxPoint inPanel = ScreenToClient(ClientToScreen(wxPoint(xClient, 0)));
        x = inPanel.x;
        y = GetPosition().y + kActionBarInset;
    }

    m_actionBar->Move(x, y);
    if (!m_actionBar->IsShown()) {
        m_actionBar->Show();
    }
}

void AiChatView::hideActionBar() {
    m_barMessage = -1;
    m_barIndex = -1;
    if (m_actionBar != nullptr && m_actionBar->IsShown()) {
        m_actionBar->Hide();
    }
}

void AiChatView::onScroll(wxScrollWinEvent& event) {
    event.Skip(); // let wxScrolled perform the actual scroll first
    if (m_barMessage >= 0 && m_barIndex >= 0) {
        // Reposition the action bar inline — the block's top edge may
        // have crossed in / out of the viewport, switching the bar between
        // the attached and detached modes. Done synchronously so we don't
        // queue an extra paint cycle per scroll tick.
        showActionBar(m_barMessage, m_barIndex, m_actionBar->mode());
    }
}

void AiChatView::onMouseWheel(wxMouseEvent& event) {
    // Horizontal trackpad swipe — route directly to the overflowing
    // block under the pointer. Uses the same fractional accumulator
    // trick as vertical scroll so macOS trackpad momentum tails don't
    // get rounded away.
    if (event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL) {
        const int wheelDelta = event.GetWheelDelta();
        if (wheelDelta <= 0) {
            event.Skip();
            return;
        }
        if (const auto target = overflowingBlockAt(event.GetPosition())) {
            constexpr int kHorizPxPerNotch = 60;
            constexpr int kDamperNum = 9;
            constexpr int kDamperDen = 10;
            const int divisor = wheelDelta * kDamperDen;
            m_hwheelPixelAccum += event.GetWheelRotation() * kHorizPxPerNotch * kDamperNum;
            const int pixels = m_hwheelPixelAccum / divisor;
            m_hwheelPixelAccum -= pixels * divisor;
            if (pixels == 0) {
                return;
            }
            const auto& item = m_items.at(target->messageIndex);
            const int current = item.blockScroll.at(target->blockIndex);
            // Positive rotation = swipe / wheel-tilt right → reveal
            // content to the right → increase scrollOffset.
            setBlockScrollOffset(target->messageIndex, target->blockIndex, current + pixels);
            return;
        }
        event.Skip();
        return;
    }
    // Vertical wheel only past here — horizontal already handled above.
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        event.Skip();
        return;
    }
    // Shift + vertical wheel over an overflowing non-wrapped code /
    // patch block scrolls that block horizontally instead of the
    // conversation — mirrors the browser convention. Plain wheel
    // keeps its conversation-scroll behaviour even over a block.
    if (event.ShiftDown()) {
        if (const auto target = overflowingBlockAt(event.GetPosition())) {
            constexpr int kScrollbarWheelStep = 40;
            const int rotation = event.GetWheelRotation();
            const int delta = (rotation > 0 ? -1 : 1) * kScrollbarWheelStep;
            const auto& item = m_items.at(target->messageIndex);
            const int current = item.blockScroll.at(target->blockIndex);
            setBlockScrollOffset(target->messageIndex, target->blockIndex, current + delta);
            return;
        }
    }

    // Rotation-to-pixels with a fractional accumulator. The default
    // wxScrolled handler quantises rotation through `wheelDelta` into
    // "lines", then scales by `linesPerAction * scrollRate` — with our
    // pixel-grain rate that becomes ~3 px per accumulated line, which:
    //   - makes a wheel notch feel sluggish, and
    //   - drops the small-rotation events macOS emits during the
    //     momentum tail of a trackpad flick.
    // Here we convert rotation directly to pixels at a fixed ratio
    // (one wheel notch ≈ 3 body line-heights) and carry the remainder
    // between events so no fraction is lost. The 9/10 damper applied
    // through the accumulator slows scroll by ~10% without losing
    // precision in the carry.
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
        return; // nothing to do this tick — fractional momentum keeps building
    }
    int viewX = 0;
    int viewY = 0;
    GetViewStart(&viewX, &viewY);
    Scroll(viewX, std::max(0, viewY - pixels));
    // Programmatic `Scroll` doesn't fire EVT_SCROLLWIN, so `onScroll`'s
    // action-bar reposition never runs from here. Without this call the
    // bar drifts off its block (it gets scrolled along with the content
    // as a child window) and the action bar lingers at a stale Y.
    if (m_barMessage >= 0 && m_barIndex >= 0) {
        showActionBar(m_barMessage, m_barIndex, m_actionBar->mode());
    }
}

void AiChatView::setBlockScrollOffset(const std::size_t messageIndex, const std::size_t index, const int offset) {
    if (messageIndex >= m_items.size()) {
        return;
    }
    auto& item = m_items.at(messageIndex);
    if (index >= item.blockScroll.size()) {
        return;
    }
    const auto& block = item.document.laid().scrollBlocks.at(index);
    const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
    const int clamped = std::clamp(offset, 0, maxScroll);
    if (item.blockScroll.at(index) == clamped) {
        return;
    }
    item.blockScroll.at(index) = clamped;
    Refresh();
}

auto AiChatView::scrollbarAt(const wxPoint& clientPoint) -> std::optional<ScrollbarTarget> {
    // Scrollbar geometry mirrors what paintMessage draws — 6px high
    // track at the bottom of each overflowing non-wrapped block; the
    // thumb width / position is proportional to contentWidth /
    // naturalWidth and the current scroll offset.
    constexpr int kScrollbarHeight = 6;
    constexpr int kScrollbarMinThumb = 24;
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;

    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        const auto& item = m_items.at(mi);
        const int contentLeftPx = item.bubble.x + kBubblePad;
        const int contentTopPx = item.bubble.y + kBubblePad - originY;
        const auto& laid = item.document.laid();
        for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
            const auto& block = laid.scrollBlocks.at(i);
            if (block.wrapped || block.naturalWidth <= block.contentWidth) {
                continue;
            }
            // Track stretches edge-to-edge across the bubble interior —
            // matches what `paintMessage` draws.
            const int trackX = contentLeftPx;
            const int trackY = contentTopPx + block.y + block.height - kScrollbarHeight;
            const int trackW = item.contentWidth;
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
            const int thumbX = trackX + (maxScroll > 0 ? (item.blockScroll.at(i) * travel / maxScroll) : 0);
            return ScrollbarTarget {
                .messageIndex = mi,
                .blockIndex = i,
                .maxScroll = maxScroll,
                .trackX = trackX,
                .trackY = trackY,
                .trackW = trackW,
                .thumbX = thumbX,
                .thumbW = thumbW,
            };
        }
    }
    return std::nullopt;
}

auto AiChatView::overflowingBlockAt(const wxPoint& clientPoint) -> std::optional<BlockTarget> {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        const auto& item = m_items.at(mi);
        const int contentLeftPx = item.bubble.x + kBubblePad;
        const int contentTopPx = item.bubble.y + kBubblePad - originY;
        const auto& laid = item.document.laid();
        for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
            const auto& block = laid.scrollBlocks.at(i);
            if (block.wrapped || block.naturalWidth <= block.contentWidth) {
                continue;
            }
            const int blockTopPx = contentTopPx + block.y;
            if (clientPoint.y < blockTopPx || clientPoint.y >= blockTopPx + block.height) {
                continue;
            }
            const int leftPx = contentLeftPx + block.contentLeft;
            if (clientPoint.x < leftPx || clientPoint.x >= leftPx + block.contentWidth) {
                continue;
            }
            return BlockTarget { .messageIndex = mi, .blockIndex = i };
        }
    }
    return std::nullopt;
}

auto AiChatView::hitTestBubble(const wxPoint& clientPoint) -> std::pair<int, SelectionPosition> {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);
    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        if (m_items.at(mi).bubble.Contains(docPoint)) {
            return { static_cast<int>(mi), hitTestInBubble(mi, clientPoint) };
        }
    }
    return { -1, SelectionPosition {} };
}

auto AiChatView::hitTestInBubble(const std::size_t messageIndex, const wxPoint& clientPoint) -> SelectionPosition {
    if (messageIndex >= m_items.size()) {
        return {};
    }
    const auto& item = m_items.at(messageIndex);
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int contentLeft = item.bubble.x + kBubblePad;
    const int contentTop = item.bubble.y + kBubblePad - originY;
    int relX = clientPoint.x - contentLeft;
    const int relY = clientPoint.y - contentTop;
    const auto& laid = item.document.laid();
    if (laid.lines.empty()) {
        return {};
    }
    // Snap to the last line whose top is at-or-above the pointer.
    // Pointer in a block gap snaps to the line above instead of jumping
    // to the end of content; pointer above every line keeps the default
    // `0`; pointer past every line keeps the last line.
    std::size_t lineIdx = 0;
    for (std::size_t i = 0; i < laid.lines.size(); i++) {
        if (laid.lines.at(i).y <= relY) {
            lineIdx = i;
        } else {
            break;
        }
    }
    // Add the block's horizontal scroll offset so per-character hit
    // testing on the shifted line lands on the visible character.
    const auto& targetLine = laid.lines.at(lineIdx);
    if (targetLine.blockIndex >= 0
        && static_cast<std::size_t>(targetLine.blockIndex) < laid.scrollBlocks.size()) {
        const auto& block = laid.scrollBlocks.at(static_cast<std::size_t>(targetLine.blockIndex));
        if (!block.wrapped) {
            relX += item.blockScroll.at(static_cast<std::size_t>(targetLine.blockIndex));
        }
    }
    const wxClientDC clientDc(this);
    wxGCDC measureDc;
    measureDc.SetGraphicsContext(makeChatGraphicsContext(clientDc));
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);
    const auto [runIdx, charIdx] = hitTestLine(laid.lines.at(lineIdx), relX, measurer);
    return { .lineIndex = lineIdx, .runIndex = runIdx, .charInRun = charIdx };
}

void AiChatView::clearSelection() {
    if (m_selectionMessage < 0 && m_selection.empty()) {
        return;
    }
    m_selectionMessage = -1;
    m_selection.clear();
    Refresh();
}

void AiChatView::copySelectionToClipboard() {
    if (m_selectionMessage < 0 || m_selection.empty()) {
        return;
    }
    const auto& laid = m_items.at(static_cast<std::size_t>(m_selectionMessage)).document.laid();
    const wxString text = extractSelectedText(laid, m_selection);
    if (text.empty()) {
        return;
    }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(text));
        wxTheClipboard->Close();
    }
}

void AiChatView::onMotion(wxMouseEvent& event) {
    const wxPoint pos = event.GetPosition();

    // Scrollbar-thumb drag — translate pointer travel back into block
    // coordinates and update the offset. Runs ahead of every other
    // motion handler so a drag started on a track keeps tracking even
    // when the pointer leaves the track.
    if (m_dragScrollMessageIndex >= 0 && event.LeftIsDown()) {
        const std::size_t mi = static_cast<std::size_t>(m_dragScrollMessageIndex);
        if (mi < m_items.size()) {
            const std::size_t idx = m_dragScrollBlockIndex;
            const auto& block = m_items.at(mi).document.laid().scrollBlocks.at(idx);
            const int maxScroll = std::max(0, block.naturalWidth - block.contentWidth);
            if (maxScroll > 0) {
                const double ratio = static_cast<double>(block.contentWidth) / static_cast<double>(block.naturalWidth);
                const int trackW = block.contentWidth;
                const int thumbW = std::max(24, static_cast<int>(trackW * ratio));
                const int travel = std::max(1, trackW - thumbW);
                const int deltaPx = pos.x - m_dragScrollStartMouseX;
                const int newOffset = m_dragScrollStartOffset + (deltaPx * maxScroll / travel);
                setBlockScrollOffset(mi, idx, newOffset);
            }
        }
        event.Skip();
        return;
    }

    // While drag-selecting, hijack the cursor and skip the action-bar
    // hover logic — the user is clearly selecting, not hovering for
    // the toolbar.
    if (m_dragSelecting && event.LeftIsDown() && m_selectionMessage >= 0) {
        m_selection.caret = hitTestInBubble(static_cast<std::size_t>(m_selectionMessage), pos);
        Refresh();
        event.Skip();
        return;
    }

    // Scrollbar hover — sample once and update the cached state.
    // Repaint when it changes so the thumb's alpha follows the
    // pointer. Cursor becomes a plain arrow over the track so the
    // I-beam doesn't suggest text selection there.
    const auto sbHit = scrollbarAt(pos);
    const int prevHoverMsg = m_hoverScrollMessageIndex;
    const std::size_t prevHoverBlock = m_hoverScrollBlockIndex;
    if (sbHit) {
        m_hoverScrollMessageIndex = static_cast<int>(sbHit->messageIndex);
        m_hoverScrollBlockIndex = sbHit->blockIndex;
    } else {
        m_hoverScrollMessageIndex = -1;
    }
    const bool hoverChanged = (prevHoverMsg != m_hoverScrollMessageIndex)
                           || (m_hoverScrollMessageIndex >= 0 && prevHoverBlock != m_hoverScrollBlockIndex);
    if (hoverChanged) {
        Refresh();
    }
    if (sbHit) {
        SetCursor(wxCursor(wxCURSOR_ARROW));
        event.Skip();
        return;
    }

    // Idle cursor — link wins over text wins over arrow.
    if (!linkAt(pos).empty()) {
        SetCursor(wxCursor(wxCURSOR_HAND));
    } else if (hitTestBubble(pos).first >= 0) {
        SetCursor(wxCursor(wxCURSOR_IBEAM));
    } else {
        SetCursor(wxCursor(wxCURSOR_ARROW));
    }

    // Try a code block first; if none is hit, fall through to patch
    // proposals. Most replies have at most one of either kind in a given
    // bubble, so the search cost is negligible.
    const auto [codeMi, codeIdx] = codeBlockAt(pos);
    if (codeMi >= 0) {
        if (codeMi != m_barMessage || codeIdx != m_barIndex
            || m_actionBar->mode() != CodeActionBar::Mode::CodeSample
            || !m_actionBar->IsShown()) {
            showActionBar(codeMi, codeIdx, CodeActionBar::Mode::CodeSample);
        }
        event.Skip();
        return;
    }
    const auto [patchMi, patchIdx] = patchBlockAt(pos);
    if (patchMi >= 0) {
        if (patchMi != m_barMessage || patchIdx != m_barIndex
            || m_actionBar->mode() != CodeActionBar::Mode::PatchProposal
            || !m_actionBar->IsShown()) {
            showActionBar(patchMi, patchIdx, CodeActionBar::Mode::PatchProposal);
        }
        event.Skip();
        return;
    }
    hideActionBar();
    event.Skip();
}

void AiChatView::onLeftDown(wxMouseEvent& event) {
    const wxPoint pos = event.GetPosition();

    // Scrollbar click — starts a drag and (on a track-not-thumb click)
    // jumps the thumb so the click point centres it. Suppresses both
    // link activation and selection so users can scrub the bar without
    // selecting any text under it.
    if (const auto hit = scrollbarAt(pos)) {
        const bool onThumb = pos.x >= hit->thumbX && pos.x < hit->thumbX + hit->thumbW;
        if (!onThumb && hit->maxScroll > 0) {
            const int travel = std::max(1, hit->trackW - hit->thumbW);
            const int newThumbX = std::clamp(pos.x - (hit->thumbW / 2),
                hit->trackX,
                hit->trackX + travel);
            const int jumpOffset = (newThumbX - hit->trackX) * hit->maxScroll / travel;
            setBlockScrollOffset(hit->messageIndex, hit->blockIndex, jumpOffset);
        }
        m_dragScrollMessageIndex = static_cast<int>(hit->messageIndex);
        m_dragScrollBlockIndex = hit->blockIndex;
        m_dragScrollStartOffset = m_items.at(hit->messageIndex).blockScroll.at(hit->blockIndex);
        m_dragScrollStartMouseX = pos.x;
        if (!HasCapture()) {
            CaptureMouse();
        }
        return;
    }

    const wxString url = linkAt(pos);
    if (!url.empty()) {
        if (isSafeLinkUrl(url)) {
            wxLaunchDefaultBrowser(url);
        } else {
            wxLogStatus("Refused to open link with unsafe scheme: %s", url);
        }
        event.Skip();
        return;
    }
    const auto [bubbleIdx, position] = hitTestBubble(pos);
    if (bubbleIdx < 0) {
        // Click in the gutter outside any bubble — drop the selection.
        clearSelection();
        event.Skip();
        return;
    }
    if (event.ShiftDown() && m_selectionMessage == bubbleIdx && !m_selection.empty()) {
        m_selection.caret = position;
    } else {
        // New selection: start anchored at the click point.
        m_selectionMessage = bubbleIdx;
        m_selection.anchor = position;
        m_selection.caret = position;
    }
    m_dragSelecting = true;
    if (!HasCapture()) {
        CaptureMouse();
    }
    SetFocus(); // so Ctrl+C reaches us
    Refresh();
}

void AiChatView::onLeftUp(wxMouseEvent& event) {
    if (m_dragScrollMessageIndex >= 0) {
        m_dragScrollMessageIndex = -1;
        if (HasCapture()) {
            ReleaseMouse();
        }
        event.Skip();
        return;
    }
    if (m_dragSelecting) {
        if (m_selectionMessage >= 0) {
            m_selection.caret = hitTestInBubble(static_cast<std::size_t>(m_selectionMessage), event.GetPosition());
        }
        m_dragSelecting = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
        Refresh();
    }
    event.Skip();
}

void AiChatView::onLeftDClick(wxMouseEvent& event) {
    const auto [bubbleIdx, position] = hitTestBubble(event.GetPosition());
    if (bubbleIdx < 0) {
        event.Skip();
        return;
    }
    const auto& laid = m_items.at(static_cast<std::size_t>(bubbleIdx)).document.laid();
    if (position.lineIndex >= laid.lines.size()) {
        event.Skip();
        return;
    }
    const auto& line = laid.lines.at(position.lineIndex);
    if (position.runIndex >= line.runs.size()) {
        event.Skip();
        return;
    }
    const auto& run = line.runs.at(position.runIndex);
    const auto isWord = [](const wxUniChar ch) {
        return wxIsalnum(ch) || ch == '_';
    };
    std::size_t start = position.charInRun;
    while (start > 0 && isWord(run.text.GetChar(start - 1))) {
        start--;
    }
    std::size_t end = position.charInRun;
    while (end < run.text.length() && isWord(run.text.GetChar(end))) {
        end++;
    }
    m_selectionMessage = bubbleIdx;
    m_selection.anchor = { .lineIndex = position.lineIndex, .runIndex = position.runIndex, .charInRun = start };
    m_selection.caret = { .lineIndex = position.lineIndex, .runIndex = position.runIndex, .charInRun = end };
    Refresh();
}

void AiChatView::onCharHook(wxKeyEvent& event) {
    if (event.ControlDown() || event.CmdDown()) {
        if (event.GetKeyCode() == 'C') {
            copySelectionToClipboard();
            return;
        }
        if (event.GetKeyCode() == 'A' && m_selectionMessage >= 0) {
            const auto& laid = m_items.at(static_cast<std::size_t>(m_selectionMessage)).document.laid();
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

void AiChatView::onLeaveWindow(wxMouseEvent& event) {
    // The pointer may have moved onto the action bar (a child window) — keep
    // the bar shown in that case.
    if (m_actionBar->IsShown()) {
        const wxRect barRect(m_actionBar->GetScreenPosition(), m_actionBar->GetSize());
        if (!barRect.Contains(wxGetMousePosition())) {
            hideActionBar();
        }
    }
    // Drop scrollbar hover highlight when pointer leaves the window
    // entirely so the active-thumb tint doesn't linger.
    if (m_hoverScrollMessageIndex >= 0) {
        m_hoverScrollMessageIndex = -1;
        Refresh();
    }
    event.Skip();
}

void AiChatView::onBarLeave(wxCommandEvent& /*event*/) {
    // The bar reported the pointer left it — drop the bar unless the pointer
    // moved back onto a code block.
    if (codeBlockAt(ScreenToClient(wxGetMousePosition())).first < 0) {
        hideActionBar();
    }
}

void AiChatView::onCopyCode(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barIndex < 0) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barMessage)];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barIndex));
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(code));
        wxTheClipboard->Close();
    }
}

void AiChatView::onInsertCode(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barIndex < 0) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barMessage)];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barIndex));
    auto* document = m_ctx.getDocumentManager().getActive();
    if (document == nullptr) {
        // No open document — drop the snippet into a fresh one.
        m_ctx.getDocumentManager().newFile().getEditor()->SetText(code);
        return;
    }
    auto* editor = document->getEditor();
    editor->BeginUndoAction();
    editor->ReplaceSelection(code);
    editor->EndUndoAction();
}

void AiChatView::onRunCode(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barIndex < 0) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barMessage)];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barIndex));
    // Open the snippet as a new document and quick-run it (compile to a temp
    // file and execute) — the same path as the Run command.
    m_ctx.getDocumentManager().newFile().getEditor()->SetText(code);
    m_ctx.getCompilerManager().quickRun();
}

auto AiChatView::applyPatch(const LaidScrollBlock& patch) -> bool {
    auto* document = m_ctx.getDocumentManager().getActive();
    if (document == nullptr) {
        return false;
    }
    auto* editor = document->getEditor();

    // Pure matching with the trailing-newline fallback lives in
    // findPatchMatch — see PatchTests for the contract.
    const auto sourceUtf8 = editor->GetText().utf8_string();
    const auto match = findPatchMatch(sourceUtf8, patch.patchSearch, patch.patchReplace);
    if (match.offset < 0) {
        return false;
    }

    editor->BeginUndoAction();
    editor->SetTargetStart(match.offset);
    editor->SetTargetEnd(match.offset + match.length);
    editor->ReplaceTarget(match.replacement);
    editor->EndUndoAction();
    return true;
}

void AiChatView::autoApplyPatches() {
    for (const auto& item : m_items) {
        for (const auto& block : item.document.laid().scrollBlocks) {
            if (block.kind != LaidScrollBlock::Kind::Patch) {
                continue;
            }
            if (!m_appliedPatches.insert(patchKey(block)).second) {
                continue; // already handled this proposal
            }
            // Apply may fail (e.g. SEARCH text not in the buffer) — the
            // hash is still inserted above, so we don't retry every
            // reparse. The proposal card remains visible for inspection.
            (void)applyPatch(block);
        }
    }
}

void AiChatView::onApplyPatch(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barIndex < 0) {
        return;
    }
    const auto& patch = m_items[static_cast<std::size_t>(m_barMessage)]
                            .document.laid()
                            .scrollBlocks[static_cast<std::size_t>(m_barIndex)];
    if (!applyPatch(patch)) {
        wxLogStatus("Patch couldn't be applied — SEARCH text not found or no active document.");
        return;
    }
    // Record so live-edit doesn't try to apply it again on the next reparse.
    m_appliedPatches.insert(patchKey(patch));
    hideActionBar();
    Refresh(); // pick up the new "applied" overlay on the card
}

void AiChatView::onRejectPatch(wxCommandEvent& /*event*/) {
    // No editor mutation — just dismiss the bar. The proposal card stays
    // visible in the chat history as an audit trail.
    hideActionBar();
}
