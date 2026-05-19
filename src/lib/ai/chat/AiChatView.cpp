//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatView.hpp"
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/settings.h>
#include "CodeHighlighter.hpp"
#include "Markdown.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
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

AiChatView::AiChatView(wxWindow* parent, Context& ctx)
: wxScrolled<wxWindow>(parent, wxID_ANY)
, m_ctx(ctx) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, kScrollStep);

    m_bodyFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    m_monoFont = m_ctx.getTheme().getResolvedFont();

    m_highlighter = std::make_unique<CodeHighlighter>(
        m_ctx.getConfigManager().keywords().at("groups")
    );

    Bind(wxEVT_PAINT, &AiChatView::onPaint, this);
    Bind(wxEVT_SIZE, &AiChatView::onSize, this);
}

AiChatView::~AiChatView() = default;

void AiChatView::setMessages(std::vector<ChatViewMessage> messages) {
    m_messages = std::move(messages);
    m_layoutWidth = -1; // force a relayout even if the width is unchanged
    relayout();
    Refresh();
}

void AiChatView::refreshTheme() {
    // Keyword groups may have changed — rebuild the configured lexer.
    m_highlighter = std::make_unique<CodeHighlighter>(
        m_ctx.getConfigManager().keywords().at("groups")
    );
    m_monoFont = m_ctx.getTheme().getResolvedFont();
    m_layoutWidth = -1;
    relayout();
    Refresh();
}

void AiChatView::onSize(wxSizeEvent& event) {
    relayout();
    Refresh();
    event.Skip();
}

void AiChatView::relayout() {
    const int panelWidth = GetClientSize().GetWidth();
    if (panelWidth == m_layoutWidth) {
        return; // width unchanged — cached layouts still valid
    }

    wxClientDC clientDc(this);
    wxGCDC measureDc(clientDc);
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont);

    const ChatPalette pal = palette();
    const auto highlight = [this](const wxString& code, const wxString& lang) {
        return highlightFence(code, lang);
    };

    // A bubble may take at most kBubbleMaxFraction of the inter-margin width,
    // leaving a gutter on the opposite side.
    const int available = std::max(120, panelWidth - (2 * kMargin));
    const int maxBubble = std::max(100, static_cast<int>(available * kBubbleMaxFraction));
    const int maxContent = std::max(kMinBubbleContent, maxBubble - (2 * kBubblePad));

    m_items.clear();
    int y = kMargin;
    for (const auto& message : m_messages) {
        LaidMessage item;
        item.fromUser = message.fromUser;
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

        const int bubbleWidth = item.contentWidth + (2 * kBubblePad);
        const int bubbleHeight = item.doc.height + (2 * kBubblePad);
        const int bubbleX = message.fromUser
                              ? (panelWidth - kMargin - bubbleWidth)
                              : kMargin;
        item.bubble = wxRect(bubbleX, y, bubbleWidth, bubbleHeight);

        y += bubbleHeight + kMessageGap;
        m_items.push_back(std::move(item));
    }

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

    // Paint into an off-screen buffer through a wxGCDC for antialiased text,
    // then blit — avoids flicker while scrolling.
    wxBitmap buffer(size);
    wxMemoryDC memoryDc(buffer);
    wxGCDC gc(memoryDc);
    gc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    gc.Clear();

    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int viewTop = originY;
    const int viewBottom = originY + size.GetHeight();

    for (const auto& item : m_items) {
        if (item.bubble.GetBottom() < viewTop || item.bubble.GetTop() > viewBottom) {
            continue; // bubble entirely outside the viewport
        }
        paintMessage(gc, item, originY);
    }

    paintDc.Blit(0, 0, size.GetWidth(), size.GetHeight(), &memoryDc, 0, 0);
}

void AiChatView::paintMessage(wxGCDC& gc, const LaidMessage& message, const int originY) const {
    const ChatPalette pal = palette();

    // Bubble — a rounded rect, then clip content to it.
    wxRect bubble = message.bubble;
    bubble.y -= originY;
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(bubbleColour(message.fromUser)));
    gc.DrawRoundedRectangle(bubble, kBubbleRadius);
    gc.SetClippingRegion(bubble);

    const int contentLeft = bubble.x + kBubblePad;
    const int contentTop = bubble.y + kBubblePad;

    for (const auto& line : message.doc.lines) {
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
            gc.SetFont(fontFor(run.style, m_bodyFont, m_monoFont));
            gc.SetTextForeground(run.colour);
            gc.DrawText(run.text, contentLeft + run.x, lineTop + 2);
        }
    }

    gc.DestroyClippingRegion();
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

auto AiChatView::highlightFence(const wxString& code, const wxString& lang) const
    -> std::vector<CodeLine> {
    if (isFreeBasicTag(lang)) {
        return m_highlighter->highlight(code, m_ctx.getTheme());
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
