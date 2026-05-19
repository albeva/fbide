//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatView.hpp"
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/settings.h>
#include <wx/utils.h>
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
// Code-block hover toolbar geometry.
constexpr int kBtnW = 54;
constexpr int kBtnH = 20;
constexpr int kBtnGap = 4;
constexpr int kToolbarInset = 6;
// Toolbar button count and labels (index order: Copy, Insert, Run).
constexpr int kButtonCount = 3;
constexpr std::array<const char*, kButtonCount> kButtonLabels { "Copy", "Insert", "Run" };

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

    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);

    Bind(wxEVT_PAINT, &AiChatView::onPaint, this);
    Bind(wxEVT_SIZE, &AiChatView::onSize, this);
    Bind(wxEVT_MOTION, &AiChatView::onMotion, this);
    Bind(wxEVT_LEFT_DOWN, &AiChatView::onLeftDown, this);
    Bind(wxEVT_LEAVE_WINDOW, &AiChatView::onLeaveWindow, this);
}

AiChatView::~AiChatView() = default;

void AiChatView::setMessages(std::vector<ChatViewMessage> messages) {
    m_messages = std::move(messages);
    relayout();                                   // per-message caching re-lays only what actually changed
    Scroll(0, (m_totalHeight / kScrollStep) + 1); // keep pinned to the newest
    Refresh();
}

void AiChatView::refreshTheme() {
    // Keyword groups may have changed — rebuild the configured lexer.
    m_highlighter = std::make_unique<CodeHighlighter>(m_ctx);
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
    paintCodeToolbar(gc, originY);

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

auto AiChatView::toolbarButtonRect(
    const LaidMessage& message,
    const LaidCodeBlock& block,
    const int button,
    const int originY
) const -> wxRect {
    const int contentLeft = message.bubble.x + kBubblePad;
    const int contentTop = message.bubble.y + kBubblePad;
    const int codeRight = contentLeft + message.contentWidth;
    const int totalWidth = (kButtonCount * kBtnW) + ((kButtonCount - 1) * kBtnGap);
    const int toolbarX = codeRight - kToolbarInset - totalWidth;
    const int x = toolbarX + (button * (kBtnW + kBtnGap));
    const int yDoc = contentTop + block.y + kToolbarInset;
    return { x, yDoc - originY, kBtnW, kBtnH };
}

auto AiChatView::hitTest(const wxPoint& clientPoint) const -> HitResult {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);

    for (std::size_t mi = 0; mi < m_items.size(); mi++) {
        const auto& item = m_items[mi];
        if (!item.bubble.Contains(docPoint)) {
            continue;
        }
        const int messageIndex = static_cast<int>(mi);

        // Code-block toolbar buttons (screen coordinates).
        for (std::size_t ci = 0; ci < item.doc.codeBlocks.size(); ci++) {
            for (int button = 0; button < kButtonCount; button++) {
                const wxRect rect = toolbarButtonRect(item, item.doc.codeBlocks[ci], button, originY);
                if (rect.Contains(clientPoint)) {
                    return { .what = HitResult::What::CodeButton,
                        .url = {},
                        .messageIndex = messageIndex,
                        .codeBlockIndex = static_cast<int>(ci),
                        .buttonIndex = button };
                }
            }
        }

        // Link runs (document coordinates).
        const int contentLeft = item.bubble.x + kBubblePad;
        const int contentTop = item.bubble.y + kBubblePad;
        for (const auto& line : item.doc.lines) {
            for (const auto& run : line.runs) {
                if (run.linkId < 0) {
                    continue;
                }
                const wxRect runRect(contentLeft + run.x, contentTop + line.y, run.width, line.height);
                if (runRect.Contains(docPoint)) {
                    return { .what = HitResult::What::Link,
                        .url = item.doc.links[static_cast<std::size_t>(run.linkId)].url,
                        .messageIndex = messageIndex,
                        .codeBlockIndex = -1,
                        .buttonIndex = -1 };
                }
            }
        }
        break; // the point lay in this bubble; nothing actionable there
    }
    return {};
}

void AiChatView::updateHover(const wxPoint& clientPoint) {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const wxPoint docPoint(clientPoint.x, clientPoint.y + originY);

    int hoverMessage = -1;
    int hoverCode = -1;
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
                hoverMessage = static_cast<int>(mi);
                hoverCode = static_cast<int>(ci);
                break;
            }
        }
        break; // only one bubble can contain the point
    }

    const HitResult hit = hitTest(clientPoint);
    const int hoverButton = hit.what == HitResult::What::CodeButton ? hit.buttonIndex : -1;

    if (hoverMessage != m_hoverMessage || hoverCode != m_hoverCode || hoverButton != m_hoverButton) {
        m_hoverMessage = hoverMessage;
        m_hoverCode = hoverCode;
        m_hoverButton = hoverButton;
        Refresh();
    }
}

void AiChatView::onMotion(wxMouseEvent& event) {
    updateHover(event.GetPosition());
    const HitResult hit = hitTest(event.GetPosition());
    SetCursor(hit.what == HitResult::What::None ? wxCursor(wxCURSOR_ARROW) : wxCursor(wxCURSOR_HAND));
    event.Skip();
}

void AiChatView::onLeftDown(wxMouseEvent& event) {
    const HitResult hit = hitTest(event.GetPosition());
    if (hit.what == HitResult::What::Link) {
        if (!hit.url.empty()) {
            wxLaunchDefaultBrowser(hit.url);
        }
    } else if (hit.what == HitResult::What::CodeButton) {
        const auto& block = m_items[static_cast<std::size_t>(hit.messageIndex)]
                                .doc.codeBlocks[static_cast<std::size_t>(hit.codeBlockIndex)];
        switch (hit.buttonIndex) {
        case 0:
            copyCode(block.code);
            break;
        case 1:
            insertCode(block.code);
            break;
        case 2:
            runCode(block.code);
            break;
        default:
            break;
        }
    }
    event.Skip();
}

void AiChatView::onLeaveWindow(wxMouseEvent& event) {
    if (m_hoverMessage != -1 || m_hoverCode != -1 || m_hoverButton != -1) {
        m_hoverMessage = -1;
        m_hoverCode = -1;
        m_hoverButton = -1;
        Refresh();
    }
    event.Skip();
}

void AiChatView::paintCodeToolbar(wxGCDC& gc, const int originY) const {
    if (m_hoverMessage < 0 || m_hoverCode < 0) {
        return;
    }
    const auto& message = m_items[static_cast<std::size_t>(m_hoverMessage)];
    const auto& block = message.doc.codeBlocks[static_cast<std::size_t>(m_hoverCode)];

    const ChatPalette pal = palette();
    const wxColour base = blend(pal.codeBg, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 0.22);
    const wxColour hover = blend(pal.codeBg, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), 0.55);
    const wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    gc.SetFont(m_bodyFont);
    for (int button = 0; button < kButtonCount; button++) {
        const wxRect rect = toolbarButtonRect(message, block, button, originY);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(button == m_hoverButton ? hover : base));
        gc.DrawRoundedRectangle(rect, 4);

        const wxString label = kButtonLabels.at(static_cast<std::size_t>(button));
        wxCoord textWidth = 0;
        wxCoord textHeight = 0;
        gc.GetTextExtent(label, &textWidth, &textHeight);
        gc.SetTextForeground(textColour);
        gc.DrawText(label, rect.x + ((rect.width - textWidth) / 2), rect.y + ((rect.height - textHeight) / 2));
    }
}

void AiChatView::copyCode(const wxString& code) const {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(code));
        wxTheClipboard->Close();
    }
}

void AiChatView::insertCode(const wxString& code) const {
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

void AiChatView::runCode(const wxString& code) const {
    // Open the snippet as a new document and quick-run it (compile to a temp
    // file and execute) — the same path as the Run command.
    m_ctx.getDocumentManager().newFile().getEditor()->SetText(code);
    m_ctx.getCompilerManager().quickRun();
}
