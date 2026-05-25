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
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentType.hpp"
#include "editor/Editor.hpp"
#include "markdown/Markdown.hpp"
#include "markdown/MarkdownLayout.hpp"
#include "ui/UIManager.hpp"
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
/// True when `lang` (a fence tag) denotes FreeBASIC. Requires an explicit
/// tag — an untagged ```...``` fence is NOT assumed to be FreeBASIC.
/// Untagged blocks rendered as model output are typically shell commands,
/// pseudo-code, or generic snippets that the FB lexer would mangle if it
/// tried to colour them.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
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
    EVT_BUTTON(ID_BlockCollapse, AiChatView::onBlockCollapse)
    EVT_BUTTON(ID_BlockExpand, AiChatView::onBlockExpand)
    EVT_COMMAND(wxID_ANY, EVT_CODE_BAR_LEAVE, AiChatView::onBarLeave)
wxEND_EVENT_TABLE()
// clang-format on

AiChatView::AiChatView(wxWindow* parent, Context& ctx, AiManager& manager)
: wxScrolled(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_ctx(ctx)
, m_manager(manager) {
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
    m_selection.clear();
    relayout();               // per-message caching re-lays only what actually changed
    Scroll(0, m_totalHeight); // keep pinned to the newest — wxScrolled clamps to the max
    Refresh();

    if (m_manager.isLiveEdit()) {
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

void AiChatView::refreshCollapsePolicy() {
    invalidateAllLayouts();
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
    const bool remap = widthChanged && m_selection.active() && !m_selection.empty();
    ChatSelection::StableOffsets savedOffsets;
    if (remap) {
        const auto& laid = m_items.at(static_cast<std::size_t>(m_selection.messageIndex())).document.laid();
        savedOffsets = m_selection.captureOffsets(laid);
    }
    relayout();
    if (remap) {
        const auto& laid = m_items.at(static_cast<std::size_t>(m_selection.messageIndex())).document.laid();
        m_selection.restoreFromOffsets(laid, savedOffsets);
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
        item.streaming = message.streaming;

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
        // Per-block collapse query — the policy lives on the view
        // (manager.isLiveEdit / manager.isPatchApplied / user override
        // map) so the layout stays agnostic and a re-laid bubble
        // matches whatever the chat currently shows. User messages
        // never carry tool patches and rarely have collapsed code, but
        // we pass the predicate uniformly so a manual toggle works on
        // either side.
        const auto isCollapsed = [this](
            markdown::LaidScrollBlock::Kind kind,
            const wxString& lang,
            const wxString& contentA,
            const wxString& contentB
        ) {
            return isBlockCollapsed(kind, lang, contentA, contentB);
        };
        const auto resolveLang = [this](markdown::LaidScrollBlock::Kind kind, const wxString& lang) {
            return resolveLanguageDisplayName(kind, lang);
        };
        const bool documentRebuilt = item.document.setMarkdown(
            message.markdown, maxContent, measurer, pal, highlight, resolveImage, wrapCodeBlocks, isCollapsed, resolveLang
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
            // Bubble width is monotone within a conversation — content
            // grows as the model streams in, but we never let a
            // re-layout snap a bubble inward. The main case the user
            // cares about is collapsing a code / patch block: the
            // strip contributes no width of its own, and without this
            // floor the bubble would jump narrower as if the body had
            // been deleted. New content (streamed text, a wider block
            // appearing) still grows the bubble normally.
            item.contentWidth = std::clamp(std::max(widest, item.contentWidth), kMinBubbleContent, maxContent);
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
    const bool hasSelection = m_selection.active()
                           && std::cmp_equal(m_selection.messageIndex(), messageIndex)
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
            markdown::paintSelectionHighlight(gc, line, lineIdx, contentLeft - scrollX, lineTop, message.contentWidth, nextLineY, m_selection.data(), highlightColour, measurer);
        }
        if (line.kind == markdown::LineKind::CollapsedBlock
            && line.blockIndex >= 0
            && static_cast<std::size_t>(line.blockIndex) < laid.scrollBlocks.size()) {
            // CollapsedBlock lines bypass `paintLineText`; their
            // summary is assembled by `paintCollapsedSummary`, which
            // talks to the DC directly (different font, colour-coded
            // count tokens). The DC's font + foreground after that
            // call no longer match `runState`'s cached values, so a
            // following prose run whose colour happens to equal the
            // stale cache would skip its `SetTextForeground` and
            // inherit the red `-N` foreground. Invalidate the cache
            // so the next prose line force-re-sets both.
            markdown::paintCollapsedSummary(
                gc,
                line,
                laid.scrollBlocks.at(static_cast<std::size_t>(line.blockIndex)),
                contentLeft, lineTop, message.contentWidth,
                m_monoFont, m_themedFont, pal
            );
            runState.styleSet = false;
            runState.colourSet = false;
        } else {
            markdown::paintLineText(gc, line, contentLeft - scrollX, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);
        }

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
    const bool dragActive = std::cmp_equal(m_blockScroll.dragMessageIndex(), messageIndex);
    const bool hoverActive = std::cmp_equal(m_blockScroll.hoverMessageIndex(), messageIndex);
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
        const int trackY = contentTop + blockY + blockHeight - BlockScrollController::kHeight;
        const int trackW = message.contentWidth;
        const int thumbW = BlockScrollController::thumbWidth(trackW, blockContentWidth, blockNaturalWidth);
        const int maxScroll = blockNaturalWidth - blockContentWidth;
        const int travel = std::max(0, trackW - thumbW);
        const int thumbX = trackX + (maxScroll > 0 ? (scroll * travel / maxScroll) : 0);
        const unsigned char thumbAlpha = active ? BlockScrollController::kThumbActiveAlpha : BlockScrollController::kThumbAlpha;
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(wxColour(pal.text.Red(), pal.text.Green(), pal.text.Blue(), BlockScrollController::kTrackAlpha)));
        gc.DrawRectangle(trackX, trackY, trackW, BlockScrollController::kHeight);
        gc.SetBrush(wxBrush(wxColour(pal.text.Red(), pal.text.Green(), pal.text.Blue(), thumbAlpha)));
        gc.DrawRectangle(thumbX, trackY, thumbW, BlockScrollController::kHeight);
    };
    for (std::size_t i = 0; i < laid.scrollBlocks.size(); i++) {
        const auto& block = laid.scrollBlocks.at(i);
        if (block.wrapped) {
            continue;
        }
        const bool active = (dragActive && m_blockScroll.dragBlock() == i)
                         || (hoverActive && m_blockScroll.hoverBlock() == i);
        drawScrollbar(block.y, block.height, block.contentWidth, block.naturalWidth, message.blockScroll.at(i), active);
    }

    // Overlay applied SEARCH/REPLACE proposals with a translucent veil so
    // the chat thread distinguishes resolved cards from still-actionable
    // ones at a glance. Drawn after content so it dims (rather than
    // hides) the underlying strips and text.
    const auto& manager = m_manager;
    const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour overlay(windowText.Red(), windowText.Green(), windowText.Blue(), 90);
    for (const auto& block : laid.scrollBlocks) {
        if (block.kind != LaidScrollBlock::Kind::Patch
            || !manager.isPatchApplied(block.patchSearch, block.patchReplace)) {
            continue;
        }
        const int patchY = contentTop + block.y;
        if (patchY + block.height < updateTop || patchY > updateBottom) {
            continue;
        }
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(overlay));
        gc.DrawRectangle(contentLeft, patchY, message.contentWidth, block.height);
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
        // Git-style accent colours for `+N` / `-N` tokens in collapsed
        // summaries — reuse the theme's diff palette so a tuned theme
        // keeps the colour story consistent across patches and strips.
        .added = theme.getChangesAdded(),
        .removed = theme.getChangesRemoved(),
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

void AiChatView::showActionBar(const int messageIndex, const int blockIndex, const std::uint8_t buttons) {
    if (messageIndex < 0 || blockIndex < 0 || buttons == 0) {
        // Nothing to show — either no anchor, or the host decided
        // every button is filtered out (e.g. a 1-liner with no
        // toggle and a kind that doesn't appear here).
        hideActionBar();
        return;
    }
    m_barPlacement.setAnchor(messageIndex, blockIndex);
    m_actionBar->setButtons(buttons);

    const auto& item = m_items[static_cast<std::size_t>(messageIndex)];
    // `blockIndex` indexes into the unified scrollBlocks vector — the
    // bar's mode (CodeSample / PatchProposal) is set by the caller from
    // the block's kind, but the geometry comes from the same field set
    // either way.
    const auto& block = item.document.laid().scrollBlocks[static_cast<std::size_t>(blockIndex)];

    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int codeRightDoc = item.bubble.x + kBubblePad + item.contentWidth;
    const int codeTopDoc = item.bubble.y + kBubblePad + block.y;
    const auto [x, y] = ActionBarPlacement::computePosition(
        codeRightDoc, codeTopDoc, originY, GetPosition().y, m_actionBar->GetSize()
    );

    m_actionBar->Move(x, y);
    if (!m_actionBar->IsShown()) {
        m_actionBar->Show();
    }
}

void AiChatView::hideActionBar() {
    m_barPlacement.clearAnchor();
    if (m_actionBar != nullptr && m_actionBar->IsShown()) {
        m_actionBar->Hide();
    }
}

auto AiChatView::isCollapsibleBlock(const markdown::LaidScrollBlock& block) -> bool {
    // Counts here mirror the layout's `countLines` (trailing newline
    // is not a phantom row). One-line code fences and `1+1` patches
    // gain nothing from a collapsed strip — the strip itself is one
    // line — so the host hides the toggle in the action bar.
    const auto countLines = [](const wxString& text) {
        if (text.empty()) {
            return 0;
        }
        int newlines = 0;
        for (const wxUniChar ch : text) {
            if (ch == '\n') {
                newlines++;
            }
        }
        return text.EndsWith("\n") ? newlines : newlines + 1;
    };
    if (block.kind == markdown::LaidScrollBlock::Kind::Patch) {
        return countLines(block.patchSearch) > 1 || countLines(block.patchReplace) > 1;
    }
    return countLines(block.codeText) > 1;
}

auto AiChatView::buttonsFor(const std::size_t mi, const std::size_t bi) const -> std::uint8_t {
    if (mi >= m_items.size()) {
        return 0;
    }
    const auto& blocks = m_items.at(mi).document.laid().scrollBlocks;
    if (bi >= blocks.size()) {
        return 0;
    }
    const auto& block = blocks.at(bi);

    // Collapsed strips reduce to a single button — the user wants to
    // see the body, nothing else. Even one-liners reach this path
    // when the user has explicitly collapsed them via an override;
    // we let them expand back out of that state.
    if (block.collapsed) {
        return CodeActionBar::Expand;
    }

    std::uint8_t mask = 0;
    if (block.kind == markdown::LaidScrollBlock::Kind::Patch) {
        mask |= CodeActionBar::Apply | CodeActionBar::Reject;
    } else {
        mask |= CodeActionBar::Copy | CodeActionBar::Insert;
        // Compile && run only makes sense for FreeBASIC fences — the
        // run path feeds the snippet into `quickRun`, which calls
        // `fbc`. The `themed` flag is what the layout set from
        // `isFreeBasicTag(codeLang)`, so it's the canonical "this is
        // FreeBASIC source" signal without re-encoding the alias
        // list here.
        if (block.themed) {
            mask |= CodeActionBar::Run;
        }
    }
    // Collapse toggle only when the block is big enough that hiding
    // its body actually saves space. One-liners stay expanded and
    // the host never offers a toggle for them.
    if (isCollapsibleBlock(block)) {
        mask |= CodeActionBar::Collapse;
    }
    return mask;
}

void AiChatView::onScroll(wxScrollWinEvent& event) {
    const FreezeLock thaw { this };

    event.Skip(); // let wxScrolled perform the actual scroll first
    if (m_barPlacement.active()) {
        // Reposition the action bar inline — the block's top edge may
        // have crossed in / out of the viewport, switching the bar
        // between the attached and detached modes. Done synchronously
        // so we don't queue an extra paint cycle per scroll tick.
        // Visibility mask doesn't change under us; reuse it.
        showActionBar(m_barPlacement.messageIndex(), m_barPlacement.blockIndex(), m_actionBar->buttons());
    }
}

void AiChatView::onMouseWheel(wxMouseEvent& event) {
    const auto wheelAxis = event.GetWheelAxis();
    const bool isHoriz = wheelAxis == wxMOUSE_WHEEL_HORIZONTAL;
    const bool isVert = wheelAxis == wxMOUSE_WHEEL_VERTICAL;
    if (!isHoriz && !isVert) {
        event.Skip();
        return;
    }

    // Axis lock — when the pointer is over an overflowing block during
    // a continuous trackpad gesture, only the gesture's dominant axis
    // is allowed through. Suppresses off-axis drift that would
    // otherwise jiggle a code/patch block sideways during a vertical
    // conversation scroll, or move the conversation while the user is
    // clearly scrolling a block horizontally. Resolved once per event
    // so `overflowingBlockAt` is called at most a single time.
    const auto target = overflowingBlockAt(event.GetPosition());
    if (target.has_value()) {
        const auto axis = isHoriz
                            ? BlockScrollController::WheelAxis::Horizontal
                            : BlockScrollController::WheelAxis::Vertical;
        if (!m_blockScroll.acquireWheelAxis(axis)) {
            return;
        }
    }

    // Horizontal trackpad swipe — route directly to the overflowing
    // block under the pointer. Uses the same fractional accumulator
    // trick as vertical scroll so macOS trackpad momentum tails don't
    // get rounded away.
    if (isHoriz) {
        if (target.has_value()) {
            const int pixels = m_blockScroll.accumulateHorizontalWheel(event.GetWheelDelta(), event.GetWheelRotation());
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

    // Shift + vertical wheel over an overflowing non-wrapped code /
    // patch block scrolls that block horizontally instead of the
    // conversation — mirrors the browser convention. Plain wheel
    // keeps its conversation-scroll behaviour even over a block.
    if (event.ShiftDown() && target.has_value()) {
        constexpr int kScrollbarWheelStep = 40;
        const int rotation = event.GetWheelRotation();
        const int delta = (rotation > 0 ? -1 : 1) * kScrollbarWheelStep;
        const auto& item = m_items.at(target->messageIndex);
        const int current = item.blockScroll.at(target->blockIndex);
        setBlockScrollOffset(target->messageIndex, target->blockIndex, current + delta);
        return;
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
    if (m_barPlacement.active()) {
        showActionBar(m_barPlacement.messageIndex(), m_barPlacement.blockIndex(), m_actionBar->buttons());
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
            const int trackY = contentTopPx + block.y + block.height - BlockScrollController::kHeight;
            const int trackW = item.contentWidth;
            if (clientPoint.y < trackY || clientPoint.y >= trackY + BlockScrollController::kHeight) {
                continue;
            }
            if (clientPoint.x < trackX || clientPoint.x >= trackX + trackW) {
                continue;
            }
            const int thumbW = BlockScrollController::thumbWidth(trackW, block.contentWidth, block.naturalWidth);
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
    if (!m_selection.active() && m_selection.empty()) {
        return;
    }
    m_selection.clear();
    Refresh();
}

void AiChatView::copySelectionToClipboard() {
    if (!m_selection.active() || m_selection.empty()) {
        return;
    }
    const auto& laid = m_items.at(static_cast<std::size_t>(m_selection.messageIndex())).document.laid();
    const wxString text = extractSelectedText(laid, m_selection.data());
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
    if (m_blockScroll.isDragging() && event.LeftIsDown()) {
        const std::size_t mi = m_blockScroll.dragMessage();
        if (mi < m_items.size()) {
            const std::size_t idx = m_blockScroll.dragBlock();
            const auto& block = m_items.at(mi).document.laid().scrollBlocks.at(idx);
            const int newOffset = m_blockScroll.translateDrag(pos.x, block.contentWidth, block.contentWidth, block.naturalWidth);
            setBlockScrollOffset(mi, idx, newOffset);
        }
        event.Skip();
        return;
    }

    // While drag-selecting, hijack the cursor and skip the action-bar
    // hover logic — the user is clearly selecting, not hovering for
    // the toolbar.
    if (m_selection.dragging() && event.LeftIsDown() && m_selection.active()) {
        m_selection.dragCaret(hitTestInBubble(static_cast<std::size_t>(m_selection.messageIndex()), pos));
        Refresh();
        event.Skip();
        return;
    }

    // Scrollbar hover — sample once and update the cached state.
    // Repaint when it changes so the thumb's alpha follows the
    // pointer. Cursor becomes a plain arrow over the track so the
    // I-beam doesn't suggest text selection there.
    const auto sbHit = scrollbarAt(pos);
    const bool hoverChanged = sbHit
                                ? m_blockScroll.setHover(sbHit->messageIndex, sbHit->blockIndex)
                                : m_blockScroll.clearHover();
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
    // proposals. `buttonsFor` encodes the block-kind + collapsed-state
    // + 1-liner policy so the bar's `setButtons` just renders the
    // computed set without re-deriving rules.
    const auto resolve = [this](const int mi, const int bi) {
        return std::pair { static_cast<std::size_t>(mi), buttonsFor(static_cast<std::size_t>(mi), static_cast<std::size_t>(bi)) };
    };
    const auto [codeMi, codeIdx] = codeBlockAt(pos);
    if (codeMi >= 0) {
        const auto [_, mask] = resolve(codeMi, codeIdx);
        if (codeMi != m_barPlacement.messageIndex() || codeIdx != m_barPlacement.blockIndex()
            || m_actionBar->buttons() != mask
            || !m_actionBar->IsShown()) {
            showActionBar(codeMi, codeIdx, mask);
        }
        event.Skip();
        return;
    }
    const auto [patchMi, patchIdx] = patchBlockAt(pos);
    if (patchMi >= 0) {
        const auto [_, mask] = resolve(patchMi, patchIdx);
        if (patchMi != m_barPlacement.messageIndex() || patchIdx != m_barPlacement.blockIndex()
            || m_actionBar->buttons() != mask
            || !m_actionBar->IsShown()) {
            showActionBar(patchMi, patchIdx, mask);
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
        m_blockScroll.beginDrag(
            hit->messageIndex,
            hit->blockIndex,
            m_items.at(hit->messageIndex).blockScroll.at(hit->blockIndex),
            pos.x
        );
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
    if (event.ShiftDown() && m_selection.messageIndex() == bubbleIdx && !m_selection.empty()) {
        m_selection.extendCaret(position);
        m_selection.setDragging(true);
    } else {
        // New selection: start anchored at the click point.
        m_selection.begin(bubbleIdx, position);
    }
    if (!HasCapture()) {
        CaptureMouse();
    }
    SetFocus(); // so Ctrl+C reaches us
    Refresh();
}

void AiChatView::onLeftUp(wxMouseEvent& event) {
    if (m_blockScroll.isDragging()) {
        m_blockScroll.endDrag();
        if (HasCapture()) {
            ReleaseMouse();
        }
        event.Skip();
        return;
    }
    if (m_selection.dragging()) {
        if (m_selection.active()) {
            m_selection.dragCaret(hitTestInBubble(static_cast<std::size_t>(m_selection.messageIndex()), event.GetPosition()));
        }
        m_selection.setDragging(false);
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
    m_selection.selectWord(bubbleIdx, position, m_items.at(static_cast<std::size_t>(bubbleIdx)).document.laid());
    Refresh();
}

void AiChatView::onCharHook(wxKeyEvent& event) {
    if (event.ControlDown() || event.CmdDown()) {
        if (event.GetKeyCode() == 'C') {
            copySelectionToClipboard();
            return;
        }
        if (event.GetKeyCode() == 'A' && m_selection.active()) {
            const auto bubbleIdx = m_selection.messageIndex();
            m_selection.selectAll(bubbleIdx, m_items.at(static_cast<std::size_t>(bubbleIdx)).document.laid());
            Refresh();
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
    if (m_blockScroll.clearHover()) {
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
    if (!m_barPlacement.active()) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barPlacement.messageIndex())];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barPlacement.blockIndex()));
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(code));
        wxTheClipboard->Close();
    }
}

void AiChatView::onInsertCode(wxCommandEvent& /*event*/) {
    if (!m_barPlacement.active()) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barPlacement.messageIndex())];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barPlacement.blockIndex()));
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
    if (!m_barPlacement.active()) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_barPlacement.messageIndex())];
    const wxString code = resolveCodeBlockText(message.document.markdown(), static_cast<std::size_t>(m_barPlacement.blockIndex()));
    // Open the snippet as a new document and quick-run it (compile to a temp
    // file and execute) — the same path as the Run command.
    m_ctx.getDocumentManager().newFile().getEditor()->SetText(code);
    m_ctx.getCompilerManager().quickRun();
}

void AiChatView::autoApplyPatches() {
    auto& manager = m_manager;
    for (const auto& item : m_items) {
        // Skip the in-flight bubble — its patch blocks may have partial
        // SEARCH text that would match the wrong spot in the buffer; the
        // next chunk extends the SEARCH and would generate a new key,
        // re-applying on top of the corrupted state. Wait until the
        // reply lands in history (streaming flag clears).
        if (item.streaming) {
            continue;
        }
        for (const auto& block : item.document.laid().scrollBlocks) {
            if (block.kind != LaidScrollBlock::Kind::Patch) {
                continue;
            }
            if (manager.isPatchApplied(block.patchSearch, block.patchReplace)) {
                continue; // already handled this proposal
            }
            // Apply may fail (e.g. SEARCH text not in the buffer); pass
            // `recordAlways=true` so the manager still notes the attempt
            // and live-edit doesn't retry it on the next streamed chunk.
            (void)manager.applyPatch(block.patchSearch, block.patchReplace, true);
        }
    }
}

void AiChatView::onApplyPatch(wxCommandEvent& /*event*/) {
    if (!m_barPlacement.active()) {
        return;
    }
    const auto& patch = m_items[static_cast<std::size_t>(m_barPlacement.messageIndex())]
                            .document.laid()
                            .scrollBlocks[static_cast<std::size_t>(m_barPlacement.blockIndex())];
    if (!m_manager.applyPatch(patch.patchSearch, patch.patchReplace)) {
        wxLogStatus("Patch couldn't be applied — SEARCH text not found or no active document.");
        return;
    }
    // Applied patches join the manager's applied set — the collapse
    // policy reads from that set, so a manual Apply needs to drop the
    // cached layout to take effect (the markdown text hasn't moved,
    // which is what `setMarkdown`'s cache compares).
    refreshCollapsePolicy();
}

void AiChatView::onRejectPatch(wxCommandEvent& /*event*/) {
    // No editor mutation — just dismiss the bar. The proposal card stays
    // visible in the chat history as an audit trail.
    hideActionBar();
}

void AiChatView::onBlockCollapse(wxCommandEvent& /*event*/) {
    setAnchoredBlockCollapsed(true);
}

void AiChatView::onBlockExpand(wxCommandEvent& /*event*/) {
    setAnchoredBlockCollapsed(false);
}

void AiChatView::setAnchoredBlockCollapsed(const bool collapsed) {
    if (!m_barPlacement.active()) {
        return;
    }
    const auto mi = static_cast<std::size_t>(m_barPlacement.messageIndex());
    const auto bi = static_cast<std::size_t>(m_barPlacement.blockIndex());
    if (mi >= m_items.size()) {
        return;
    }
    const auto& blocks = m_items.at(mi).document.laid().scrollBlocks;
    if (bi >= blocks.size()) {
        return;
    }
    const auto& block = blocks.at(bi);
    // The override key must match what the layout pass produced for
    // this block — same `(kind, lang, contentA, contentB)` quadruple.
    // Both shapes (code / patch) cached their content on `block` so we
    // can recompute the key without re-parsing the markdown.
    const auto key = block.kind == markdown::LaidScrollBlock::Kind::Patch
                       ? blockKey(block.kind, wxString {}, block.patchSearch, block.patchReplace)
                       : blockKey(block.kind, block.codeLang, block.codeText, wxString {});
    m_collapseOverrides[key] = collapsed;
    invalidateAllLayouts();
    relayout();
    // Keep the bar anchored on the same block so the user can toggle
    // back without re-hovering — `buttonsFor` re-computes the
    // visible set from the laid block's new `collapsed` flag so the
    // Collapse / Expand toggle switches in place.
    if (m_barPlacement.active()) {
        showActionBar(
            m_barPlacement.messageIndex(),
            m_barPlacement.blockIndex(),
            buttonsFor(mi, bi)
        );
    }
    Refresh();
}

auto AiChatView::isBlockCollapsed(
    const markdown::LaidScrollBlock::Kind kind,
    const wxString& lang,
    const wxString& contentA,
    const wxString& contentB
) const -> bool {
    const auto key = blockKey(kind, lang, contentA, contentB);
    if (const auto it = m_collapseOverrides.find(key); it != m_collapseOverrides.end()) {
        return it->second;
    }
    // Default policy:
    //   patch  → collapsed when live-edit is on (suppresses flicker
    //            during streaming auto-apply) OR when the patch has
    //            already been applied this session (manual Apply).
    //   code   → expanded; non-patch code blocks have no auto-apply
    //            flicker to hide, so the user opts in via the toggle.
    if (kind == markdown::LaidScrollBlock::Kind::Patch) {
        if (m_manager.isLiveEdit()) {
            return true;
        }
        if (m_manager.isPatchApplied(contentA, contentB)) {
            return true;
        }
    }
    return false;
}

auto AiChatView::resolveLanguageDisplayName(
    const markdown::LaidScrollBlock::Kind kind,
    const wxString& lang
) const -> wxString {
    // Look up `statusbar.type.<key>` in the active locale. Path keys
    // use dots (the Value tree splits on `.`), matching how the
    // status bar itself resolves these strings.
    const auto displayFor = [this](DocumentType type) -> wxString {
        const auto key = std::string(documentTypeKey(type));
        return m_ctx.tr(wxString { "statusbar.type." } + wxString::FromUTF8(key.data(), key.size()));
    };

    // Two input shapes:
    //  - a fence tag (`freebasic`, `json`, …) for code blocks;
    //  - a patch target path (e.g. `editor.bas`) for patches.
    if (!lang.empty()) {
        auto tag = lang;
        tag.MakeLower();
        // Aliases that aren't `DocumentType` keys themselves — `fb` /
        // `bas` / `basic` all collapse to FreeBASIC, matching the
        // layout's `isFreeBasicTag` predicate so the themed font and
        // the summary label agree.
        if (tag == "fb" || tag == "bas" || tag == "basic") {
            return displayFor(DocumentType::FreeBASIC);
        }
        if (const auto type = documentTypeFromKey(tag.utf8_string())) {
            return displayFor(*type);
        }
        // Path-shaped — let the extension drive the resolution. Common
        // for patches whose SEARCH header includes a filename.
        if (lang.Contains(".") || lang.Contains("/") || lang.Contains("\\")) {
            const auto type = documentTypeFromPath(toPath(lang));
            if (auto display = displayFor(type); !display.empty()) {
                return display;
            }
        }
        return {}; // raw tag — don't echo it, let the painter drop the wide tier
    }

    // Patches with an empty `lang` carried no target in their SEARCH
    // header — fall back to the pinned edit target so the label tracks
    // the file the patch will actually apply to. Code blocks without
    // a fence tag get no fallback (they may not be source code at all).
    if (kind == markdown::LaidScrollBlock::Kind::Patch) {
        if (const auto* target = m_manager.context().editTarget()) {
            return displayFor(documentTypeFromPath(target->path()));
        }
    }
    return {};
}

auto AiChatView::blockKey(
    const markdown::LaidScrollBlock::Kind kind,
    const wxString& lang,
    const wxString& contentA,
    const wxString& contentB
) -> std::size_t {
    const std::size_t a = std::hash<std::string> {}(contentA.utf8_string());
    const std::size_t b = std::hash<std::string> {}(contentB.utf8_string());
    const std::size_t kindHash = static_cast<std::size_t>(kind);
    const std::size_t langHash = std::hash<std::string> {}(lang.utf8_string());
    return hashCombine(hashCombine(hashCombine(kindHash, langHash), a), b);
}

void AiChatView::invalidateAllLayouts() {
    // The cache key on MarkdownDocument doesn't capture predicate state,
    // so a policy flip (collapse override, live-edit toggle, patch
    // applied) needs an explicit nudge to make `setMarkdown` rebuild.
    for (auto& item : m_items) {
        item.document.invalidate();
    }
}
