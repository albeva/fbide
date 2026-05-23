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
#include "editor/Editor.hpp"
#include "markdown/Markdown.hpp"
#include "markdown/MarkdownLayout.hpp"
using namespace fbide;

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
auto patchKey(const LaidPatchBlock& patch) -> std::string {
    return (patch.search + wxString("\n>>>\n") + patch.replace).utf8_string();
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
    // A width change re-wraps every bubble's content; the cached
    // `(line, run, char)` positions in `m_selection` would then point
    // into the wrong text. Drop the selection rather than let it drift.
    if (GetClientSize().GetWidth() != m_layoutWidth && m_selectionMessage >= 0) {
        m_selectionMessage = -1;
        m_selection.clear();
    }
    relayout();
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
        // only the last bubble's text actually moves).
        if (index < m_items.size() && m_items[index].fromUser == message.fromUser) {
            item.document = std::move(m_items[index].document);
            item.contentWidth = m_items[index].contentWidth;
        }

        // Reformat model replies; leave the user's own code untouched.
        const bool reformat = !message.fromUser;
        const auto highlight = [this, reformat](const wxString& code, const wxString& lang) {
            return highlightFence(code, lang, reformat);
        };
        const bool rebuilt_layout = item.document.setMarkdown(
            message.markdown, maxContent, measurer, pal, highlight, resolveImage
        );
        if (rebuilt_layout) {
            // Shrink the bubble to its widest line — wrapping was done
            // at maxContent, so every line already fits the shrunk width.
            int widest = 0;
            for (const auto& line : item.document.laid().lines) {
                for (const auto& run : line.runs) {
                    widest = std::max(widest, run.x + run.width);
                }
            }
            item.contentWidth = std::clamp(widest, kMinBubbleContent, maxContent);
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
        fbide::paintLineBackground(gc, line, contentLeft, lineTop, message.contentWidth, pal);
        if (hasSelection) {
            fbide::paintSelectionHighlight(gc, line, lineIdx, contentLeft, lineTop, message.contentWidth, nextLineY, m_selection, highlightColour, measurer);
        }
        fbide::paintLineText(gc, line, contentLeft, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);
    }

    // Overlay applied SEARCH/REPLACE proposals with a translucent veil so
    // the chat thread distinguishes resolved cards from still-actionable
    // ones at a glance. Drawn after content so it dims (rather than
    // hides) the underlying strips and text.
    if (!laid.patchBlocks.empty() && !m_appliedPatches.empty()) {
        const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        const wxColour overlay(windowText.Red(), windowText.Green(), windowText.Blue(), 90);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(overlay));
        for (const auto& patch : laid.patchBlocks) {
            if (!m_appliedPatches.contains(patchKey(patch))) {
                continue;
            }
            const int patchY = contentTop + patch.y;
            if (patchY + patch.height < updateTop || patchY > updateBottom) {
                continue;
            }
            gc.DrawRectangle(contentLeft, patchY, message.contentWidth, patch.height);
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
        for (std::size_t ci = 0; ci < item.document.laid().codeBlocks.size(); ci++) {
            const auto& block = item.document.laid().codeBlocks[ci];
            const wxRect codeRect(contentLeft, contentTop + block.y, item.contentWidth, block.height);
            if (codeRect.Contains(docPoint)) {
                return { static_cast<int>(mi), static_cast<int>(ci) };
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
        for (std::size_t pi = 0; pi < item.document.laid().patchBlocks.size(); pi++) {
            const auto& block = item.document.laid().patchBlocks[pi];
            const wxRect patchRect(contentLeft, contentTop + block.y, item.contentWidth, block.height);
            if (patchRect.Contains(docPoint)) {
                return { static_cast<int>(mi), static_cast<int>(pi) };
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
    // Mode picks the right block table — code blocks for CodeSample, patch
    // proposals for PatchProposal. The two share (y, height) so the
    // positioning math is identical past this point.
    int blockY = 0;
    int blockHeight = 0;
    if (mode == CodeActionBar::Mode::CodeSample) {
        const auto& block = item.document.laid().codeBlocks[static_cast<std::size_t>(blockIndex)];
        blockY = block.y;
        blockHeight = block.height;
    } else {
        const auto& block = item.document.laid().patchBlocks[static_cast<std::size_t>(blockIndex)];
        blockY = block.y;
        blockHeight = block.height;
    }
    (void)blockHeight; // reserved — bottom-pin behaviour can use it later.

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

void AiChatView::hideActionBar() {
    m_barMessage = -1;
    m_barIndex = -1;
    if (m_actionBar != nullptr && m_actionBar->IsShown()) {
        m_actionBar->Hide();
    }
}

void AiChatView::onMouseWheel(wxMouseEvent& event) {
    // Vertical wheel only — horizontal events fall through to default.
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        event.Skip();
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
    const int relX = clientPoint.x - contentLeft;
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

    // While drag-selecting, hijack the cursor and skip the action-bar
    // hover logic — the user is clearly selecting, not hovering for
    // the toolbar.
    if (m_dragSelecting && event.LeftIsDown() && m_selectionMessage >= 0) {
        m_selection.caret = hitTestInBubble(static_cast<std::size_t>(m_selectionMessage), pos);
        Refresh();
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
    const wxString url = linkAt(pos);
    if (!url.empty()) {
        wxLaunchDefaultBrowser(url);
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

auto AiChatView::applyPatch(const LaidPatchBlock& patch) -> bool {
    auto* document = m_ctx.getDocumentManager().getActive();
    if (document == nullptr) {
        return false;
    }
    auto* editor = document->getEditor();

    // Try the SEARCH text as-is first. If a model emits a block that ends
    // with `\n` (the common case) but the matching text in the buffer
    // doesn't have the trailing newline (final line, no EOL), retry with
    // both strings trimmed of their trailing newline.
    wxString search = patch.search;
    wxString replace = patch.replace;
    editor->SetTargetStart(0);
    editor->SetTargetEnd(editor->GetLength());
    editor->SetSearchFlags(wxSTC_FIND_MATCHCASE);
    int found = editor->SearchInTarget(search);
    if (found < 0 && !search.empty() && search[search.length() - 1] == '\n') {
        search.RemoveLast();
        if (!replace.empty() && replace[replace.length() - 1] == '\n') {
            replace.RemoveLast();
        }
        editor->SetTargetStart(0);
        editor->SetTargetEnd(editor->GetLength());
        found = editor->SearchInTarget(search);
    }
    if (found < 0) {
        return false;
    }

    editor->BeginUndoAction();
    editor->ReplaceTarget(replace);
    editor->EndUndoAction();
    return true;
}

void AiChatView::autoApplyPatches() {
    for (const auto& item : m_items) {
        for (const auto& patch : item.document.laid().patchBlocks) {
            if (!m_appliedPatches.insert(patchKey(patch)).second) {
                continue; // already handled this proposal
            }
            // Apply may fail (e.g. SEARCH text not in the buffer) — the
            // hash is still inserted above, so we don't retry every
            // reparse. The proposal card remains visible for inspection.
            (void)applyPatch(patch);
        }
    }
}

void AiChatView::onApplyPatch(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barIndex < 0) {
        return;
    }
    const auto& patch = m_items[static_cast<std::size_t>(m_barMessage)]
                            .document.laid()
                            .patchBlocks[static_cast<std::size_t>(m_barIndex)];
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
