//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatView.hpp"
#include <wx/sizer.h>
#include "ChatLayout.hpp"
#include "CodeActionBar.hpp"
#include "CodeHighlighter.hpp"
#include "Markdown.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
using namespace fbide;

namespace {

// Outer margin between the view edge and the bubbles.
constexpr int kMargin = 12;
// Vertical gap between consecutive message bubbles.
constexpr int kMessageGap = 10;
// Vertical scroll step in pixels.
constexpr int kScrollStep = 16;
// Padding between a bubble's edge and its content.
constexpr int kBubblePad = 10;
// Corner radius of a message bubble.
constexpr int kBubbleRadius = 10;
// Largest fraction of the available width a bubble may occupy.
constexpr double kBubbleMaxFraction = 0.85;
// Smallest content width a bubble shrinks to.
constexpr int kMinBubbleContent = 60;
// Inset of the action bar from the code block's top-right corner.
constexpr int kActionBarInset = 4;

/// True when `lang` (a fence tag) denotes FreeBASIC. An empty tag counts — in
/// a FreeBASIC IDE an untagged block is assumed to be FreeBASIC.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang.empty() || lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
}

/// Linear blend of two colours — `t` of 0 yields `a`, 1 yields `b`.
auto blend(const wxColour& a, const wxColour& b, const double t) -> wxColour {
    const auto mix = [t](const unsigned char from, const unsigned char to) {
        return static_cast<unsigned char>(from + ((to - from) * t));
    };
    return { mix(a.Red(), b.Red()), mix(a.Green(), b.Green()), mix(a.Blue(), b.Blue()) };
}

/// Resolve a concrete font for a layout text style from the body/mono bases.
auto fontFor(const TextStyle& style, const wxFont& body, const wxFont& mono) -> wxFont {
    wxFont font = style.monospace ? mono : body;
    if (style.sizeDelta != 0) {
        font.SetPointSize(std::max(4, font.GetPointSize() + style.sizeDelta));
    }
    font.SetWeight(style.bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    font.SetStyle(style.italic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
    font.SetUnderlined(style.underline);
    font.SetStrikethrough(style.strikethrough);
    return font;
}

/// `TextMeasurer` backed by a wxDC — measures with the same fonts the view
/// paints with, so layout and paint agree to the pixel.
class DcMeasurer final : public TextMeasurer {
public:
    DcMeasurer(wxDC& dc, wxFont body, wxFont mono)
    : m_dc(dc)
    , m_body(std::move(body))
    , m_mono(std::move(mono)) {}

    auto width(const wxString& text, const TextStyle& style) const -> int override {
        if (text.empty()) {
            return 0;
        }
        const wxFont font = fontFor(style, m_body, m_mono);
        wxCoord textWidth = 0;
        wxCoord textHeight = 0;
        m_dc.GetTextExtent(text, &textWidth, &textHeight, nullptr, nullptr, &font);
        return textWidth;
    }

    auto lineHeight(const TextStyle& style) const -> int override {
        const wxFont font = fontFor(style, m_body, m_mono);
        wxCoord textWidth = 0;
        wxCoord textHeight = 0;
        m_dc.GetTextExtent("Ag", &textWidth, &textHeight, nullptr, nullptr, &font);
        return textHeight + 4; // a little leading
    }

private:
    wxDC& m_dc;
    wxFont m_body;
    wxFont m_mono;
};

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(AiChatView, wxScrolled)
    EVT_PAINT(AiChatView::onPaint)
    EVT_SIZE(AiChatView::onSize)
    EVT_MOTION(AiChatView::onMotion)
    EVT_LEFT_DOWN(AiChatView::onLeftDown)
    EVT_LEAVE_WINDOW(AiChatView::onLeaveWindow)
    EVT_SCROLLWIN(AiChatView::onScroll)
    EVT_BUTTON(ID_CodeCopy, AiChatView::onCopyCode)
    EVT_BUTTON(ID_CodeInsert, AiChatView::onInsertCode)
    EVT_BUTTON(ID_CodeRun, AiChatView::onRunCode)
    EVT_COMMAND(wxID_ANY, EVT_CODE_BAR_LEAVE, AiChatView::onBarLeave)
wxEND_EVENT_TABLE()
// clang-format on

AiChatView::AiChatView(wxWindow* parent, Context& ctx)
: wxScrolled(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_ctx(ctx) {
    wxScrolled::SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, kScrollStep);

    m_bodyFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    m_monoFont = m_ctx.getTheme().getResolvedFont();
    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);

    // One reusable action bar, shown over whichever code block is hovered.
    // It is a child of this scroll surface, so it scrolls with the content.
    m_actionBar = make_unowned<CodeActionBar>(this, m_ctx);
    m_actionBar->Hide();
}

AiChatView::~AiChatView() = default;

void AiChatView::setMessages(std::vector<ChatViewMessage> messages) {
    m_messages = std::move(messages);
    relayout();                                                // per-message caching re-lays only what actually changed
    Scroll(0, m_totalHeight / kScrollStep + 1);  // keep pinned to the newest
    Refresh();
}

void AiChatView::refreshTheme() {
    // Keyword groups may have changed — rebuild the configured lexer.
    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);
    m_monoFont = m_ctx.getTheme().getResolvedFont();
    m_layoutWidth = -1;
    hideActionBar();
    relayout();
    Refresh();
}

void AiChatView::onSize(wxSizeEvent& event) {
    hideActionBar(); // bubble positions shift — re-hover brings the bar back
    relayout();
    Refresh();
    event.Skip();
}

void AiChatView::relayout() {
    const int panelWidth = GetClientSize().GetWidth();
    const bool widthChanged = panelWidth != m_layoutWidth;

    wxClientDC clientDc(this);
    wxGCDC measureDc(clientDc);
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont);

    const ChatPalette pal = palette();

    // A bubble may take at most kBubbleMaxFraction of the inter-margin width,
    // leaving a gutter on the opposite side.
    const int available = std::max(120, panelWidth - (2 * kMargin));
    const int maxBubble = std::max(100, static_cast<int>(available * kBubbleMaxFraction));
    const int maxContent = std::max(kMinBubbleContent, maxBubble - (2 * kBubblePad));

    std::vector<LaidMessage> rebuilt;
    rebuilt.reserve(m_messages.size());
    int y = kMargin;
    for (std::size_t index = 0; index < m_messages.size(); index++) {
        const auto& message = m_messages[index];
        LaidMessage item;
        item.fromUser = message.fromUser;
        item.markdown = message.markdown;

        // Reuse the cached layout when the message text and width are both
        // unchanged — while a reply streams, only the last message moves.
        const bool reusable = !widthChanged
                           && index < m_items.size()
                           && m_items[index].fromUser == message.fromUser
                           && m_items[index].markdown == message.markdown;
        if (reusable) {
            item.doc = std::move(m_items[index].doc);
            item.contentWidth = m_items[index].contentWidth;
        } else {
            // Reformat model replies; leave the user's own code untouched.
            const bool reformat = !message.fromUser;
            const auto highlight = [this, reformat](const wxString& code, const wxString& lang) {
                return highlightFence(code, lang, reformat);
            };
            item.doc = layoutMarkdown(parseMarkdown(message.markdown), maxContent, measurer, pal, highlight);
            // Shrink the bubble to its widest line — wrapping was done at
            // maxContent, so every line already fits the shrunk width.
            int widest = 0;
            for (const auto& line : item.doc.lines) {
                for (const auto& run : line.runs) {
                    widest = std::max(widest, run.x + run.width);
                }
            }
            item.contentWidth = std::clamp(widest, kMinBubbleContent, maxContent);
        }

        const int bubbleWidth = item.contentWidth + (2 * kBubblePad);
        const int bubbleHeight = item.doc.height + (2 * kBubblePad);
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
    // the window resizes. wxScrolled scrolls existing pixels and invalidates
    // just the newly-exposed strip, so most repaints touch a small region.
    if (!m_buffer.IsOk() || m_buffer.GetSize() != size) {
        m_buffer = wxBitmap(size);
    }

    const wxRect update = GetUpdateRegion().GetBox();

    wxMemoryDC memoryDc(m_buffer);
    wxGCDC gc(memoryDc);

    // Repaint only the update rect — fill the band with background, draw
    // through it, blit it. Pixels outside the rect stay as they were.
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    gc.DrawRectangle(update);

    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int regionTopDoc = originY + update.y;
    const int regionBottomDoc = regionTopDoc + update.height;

    // Bubbles are stacked in document order — binary-search to the first one
    // that can intersect the dirty band rather than scanning the whole list.
    const auto first = std::lower_bound(
        m_items.begin(), m_items.end(), regionTopDoc,
        [](const LaidMessage& message, const int top) { return message.bubble.GetBottom() < top; }
    );
    for (auto it = first; it != m_items.end(); ++it) {
        if (it->bubble.GetTop() > regionBottomDoc) {
            break; // first bubble past the band — the rest are too
        }
        paintMessage(gc, *it, originY, update.y, update.y + update.height);
    }

    paintDc.Blit(update.x, update.y, update.width, update.height, &memoryDc, update.x, update.y);
}

void AiChatView::paintMessage(
    wxGCDC& gc,
    const LaidMessage& message,
    const int originY,
    const int updateTop,
    const int updateBottom
) const {
    const ChatPalette pal = palette();

    // Bubble — a rounded rect. Content stays within the rect by layout, so
    // no per-message clipping region is needed.
    wxRect bubble = message.bubble;
    bubble.y -= originY;
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(bubbleColour(message.fromUser)));
    gc.DrawRoundedRectangle(bubble, kBubbleRadius);

    const int contentLeft = bubble.x + kBubblePad;
    const int contentTop = bubble.y + kBubblePad;

    // Binary-search to the first line that can be inside the dirty band.
    // Lines are stacked by y in the laid-out document.
    const auto& lines = message.doc.lines;
    const int updateTopRel = updateTop - contentTop;
    const int updateBottomRel = updateBottom - contentTop;
    auto first = std::lower_bound(
        lines.begin(), lines.end(), updateTopRel,
        [](const PaintLine& line, const int top) { return line.y + line.height < top; }
    );

    // Cache the last applied font / colour to avoid redundant DC state churn
    // — a paragraph's runs mostly share the body font, and adjacent runs
    // often share colour.
    TextStyle currentStyle {};
    wxColour currentColour;
    bool styleSet = false;
    bool colourSet = false;

    for (auto it = first; it != lines.end(); ++it) {
        const auto& line = *it;
        if (line.y > updateBottomRel) {
            break;
        }
        const int lineTop = contentTop + line.y;

        if (line.kind == LineKind::Code) {
            gc.SetBrush(wxBrush(pal.codeBg));
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.DrawRectangle(contentLeft, lineTop, message.contentWidth, line.height);
        } else if (line.kind == LineKind::Rule) {
            gc.SetPen(wxPen(pal.rule));
            const int ruleY = lineTop + (line.height / 2);
            gc.DrawLine(contentLeft, ruleY, contentLeft + message.contentWidth, ruleY);
        }

        for (const auto& run : line.runs) {
            if (run.text.empty()) {
                continue;
            }
            if (!styleSet || run.style != currentStyle) {
                gc.SetFont(fontFor(run.style, m_bodyFont, m_monoFont));
                currentStyle = run.style;
                styleSet = true;
            }
            if (!colourSet || run.colour != currentColour) {
                gc.SetTextForeground(run.colour);
                currentColour = run.colour;
                colourSet = true;
            }
            gc.DrawText(run.text, contentLeft + run.x, lineTop + 2);
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

auto AiChatView::palette() const -> ChatPalette {
    const auto& theme = m_ctx.getTheme();
    const wxColour separator = theme.getSeparator();
    return {
        .text = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT),
        .link = wxColour(40, 100, 220),
        .codeBg = theme.background({}),
        .inlineCodeBg = theme.background({}),
        .rule = separator.IsOk() ? separator : wxColour(180, 180, 180),
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
        for (const auto& line : item.doc.lines) {
            for (const auto& run : line.runs) {
                if (run.linkId < 0) {
                    continue;
                }
                const wxRect runRect(contentLeft + run.x, contentTop + line.y, run.width, line.height);
                if (runRect.Contains(docPoint)) {
                    return item.doc.links[static_cast<std::size_t>(run.linkId)].url;
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
        for (std::size_t ci = 0; ci < item.doc.codeBlocks.size(); ci++) {
            const auto& block = item.doc.codeBlocks[ci];
            const wxRect codeRect(contentLeft, contentTop + block.y, item.contentWidth, block.height);
            if (codeRect.Contains(docPoint)) {
                return { static_cast<int>(mi), static_cast<int>(ci) };
            }
        }
        break;
    }
    return { -1, -1 };
}

void AiChatView::showActionBar(const int messageIndex, const int codeIndex) {
    if (messageIndex < 0 || codeIndex < 0) {
        hideActionBar();
        return;
    }
    m_barMessage = messageIndex;
    m_barCode = codeIndex;

    const auto& item = m_items[static_cast<std::size_t>(messageIndex)];
    const auto& block = item.doc.codeBlocks[static_cast<std::size_t>(codeIndex)];
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int codeRight = item.bubble.x + kBubblePad + item.contentWidth;
    const int codeTopClient = item.bubble.y + kBubblePad + block.y - originY;

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
    if (m_barMessage >= 0 && m_barCode >= 0) {
        // Reposition once the scroll lands — the snippet's top edge may have
        // crossed in / out of the viewport, switching the bar between the
        // attached and detached modes.
        CallAfter([this] { showActionBar(m_barMessage, m_barCode); });
    }
}

void AiChatView::hideActionBar() {
    m_barMessage = -1;
    m_barCode = -1;
    if (m_actionBar != nullptr && m_actionBar->IsShown()) {
        m_actionBar->Hide();
    }
}

void AiChatView::onMotion(wxMouseEvent& event) {
    const wxPoint pos = event.GetPosition();
    SetCursor(linkAt(pos).empty() ? wxCursor(wxCURSOR_ARROW) : wxCursor(wxCURSOR_HAND));

    const auto [messageIndex, codeIndex] = codeBlockAt(pos);
    if (messageIndex < 0) {
        hideActionBar();
    } else if (messageIndex != m_barMessage || codeIndex != m_barCode || !m_actionBar->IsShown()) {
        showActionBar(messageIndex, codeIndex);
    }
    event.Skip();
}

void AiChatView::onLeftDown(wxMouseEvent& event) {
    const wxString url = linkAt(event.GetPosition());
    if (!url.empty()) {
        wxLaunchDefaultBrowser(url);
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
    if (m_barMessage < 0 || m_barCode < 0) {
        return;
    }
    const wxString& code = m_items[static_cast<std::size_t>(m_barMessage)]
                               .doc.codeBlocks[static_cast<std::size_t>(m_barCode)]
                               .code;
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(code));
        wxTheClipboard->Close();
    }
}

void AiChatView::onInsertCode(wxCommandEvent& /*event*/) {
    if (m_barMessage < 0 || m_barCode < 0) {
        return;
    }
    const wxString& code = m_items[static_cast<std::size_t>(m_barMessage)]
                               .doc.codeBlocks[static_cast<std::size_t>(m_barCode)]
                               .code;
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
    if (m_barMessage < 0 || m_barCode < 0) {
        return;
    }
    const wxString& code = m_items[static_cast<std::size_t>(m_barMessage)]
                               .doc.codeBlocks[static_cast<std::size_t>(m_barCode)]
                               .code;
    // Open the snippet as a new document and quick-run it (compile to a temp
    // file and execute) — the same path as the Run command.
    m_ctx.getDocumentManager().newFile().getEditor()->SetText(code);
    m_ctx.getCompilerManager().quickRun();
}

