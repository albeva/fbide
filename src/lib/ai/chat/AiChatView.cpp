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

// Outer margin between the view edge and message content.
constexpr int kMargin = 12;
// Vertical gap between consecutive messages.
constexpr int kMessageGap = 16;
// Vertical scroll step in pixels.
constexpr int kScrollStep = 16;

/// True when `lang` (a fence tag) denotes FreeBASIC. An empty tag counts — in
/// a FreeBASIC IDE an untagged block is assumed to be FreeBASIC.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang.empty() || lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
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
    const int contentWidth = std::max(80, GetClientSize().GetWidth() - (2 * kMargin));
    if (contentWidth == m_layoutWidth) {
        return; // width unchanged — cached layouts still valid
    }

    wxClientDC clientDc(this);
    wxGCDC measureDc(clientDc);
    const DcMeasurer measurer(measureDc, m_bodyFont, m_monoFont);

    const ChatPalette pal = palette();
    const auto highlight = [this](const wxString& code, const wxString& lang) {
        return highlightFence(code, lang);
    };

    m_layouts.clear();
    m_offsets.clear();
    int y = 0;
    for (const auto& message : m_messages) {
        auto doc = layoutMarkdown(parseMarkdown(message.markdown), contentWidth, measurer, pal, highlight);
        m_offsets.push_back(y);
        y += doc.height + kMessageGap;
        m_layouts.push_back(std::move(doc));
    }

    m_totalHeight = m_messages.empty() ? 0 : (y - kMessageGap);
    m_layoutWidth = contentWidth;
    SetVirtualSize(GetClientSize().GetWidth(), m_totalHeight + (2 * kMargin));
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
    const int viewTop = originY - kMargin;
    const int viewBottom = originY + size.GetHeight();

    for (std::size_t index = 0; index < m_layouts.size(); index++) {
        const int messageTop = m_offsets[index] + kMargin;
        const auto& doc = m_layouts[index];
        if (messageTop + doc.height < viewTop || messageTop > viewBottom) {
            continue; // message entirely outside the viewport
        }
        paintMessage(gc, doc, kMargin, messageTop - originY);
    }

    paintDc.Blit(0, 0, size.GetWidth(), size.GetHeight(), &memoryDc, 0, 0);
}

void AiChatView::paintMessage(wxGCDC& gc, const LaidOutDoc& doc, const int leftMargin, const int screenTop) const {
    const ChatPalette pal = palette();
    const int clientHeight = GetClientSize().GetHeight();

    for (const auto& line : doc.lines) {
        const int lineTop = screenTop + line.y;
        if (lineTop + line.height < 0 || lineTop > clientHeight) {
            continue; // line outside the viewport
        }

        if (line.kind == LineKind::Code) {
            gc.SetBrush(wxBrush(pal.codeBg));
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.DrawRectangle(leftMargin, lineTop, m_layoutWidth, line.height);
        } else if (line.kind == LineKind::Rule) {
            gc.SetPen(wxPen(pal.rule));
            const int ruleY = lineTop + (line.height / 2);
            gc.DrawLine(leftMargin, ruleY, leftMargin + m_layoutWidth, ruleY);
        }

        for (const auto& run : line.runs) {
            if (run.text.empty()) {
                continue;
            }
            gc.SetFont(fontFor(run.style, m_bodyFont, m_monoFont));
            gc.SetTextForeground(run.colour);
            gc.DrawText(run.text, leftMargin + run.x, lineTop + 2);
        }
    }
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
