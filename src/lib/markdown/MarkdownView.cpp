//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownView.hpp"
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
using namespace fbide;

// NOLINTNEXTLINE(cert-err58-cpp, bugprone-throwing-static-initialization)
wxDEFINE_EVENT(fbide::MARKDOWN_LINK_CLICKED, wxCommandEvent);

namespace {

/// Inner padding between the panel edge and content.
constexpr int kPadding = 8;
/// A touch of leading on top of the measured font height — matches the
/// chat view's calculation so wheel-step quantisation lines up.
constexpr int kBodyLeading = 4;

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

/// Linear blend of two colours — `t` of 0 yields `a`, 1 yields `b`.
auto blend(const wxColour& aColour, const wxColour& bColour, const double tee) -> wxColour {
    const auto mix = [tee](const unsigned char from, const unsigned char to) {
        return static_cast<unsigned char>(from + ((to - from) * tee));
    };
    return { mix(aColour.Red(), bColour.Red()),
        mix(aColour.Green(), bColour.Green()),
        mix(aColour.Blue(), bColour.Blue()) };
}

/// Default highlighter — one CodeLine per source line, all rendered in
/// the system's default text colour, monospace. No syntax colouring.
/// Hosts that want language-aware highlighting inject one via
/// `MarkdownView::setHighlighter`.
auto defaultHighlight(const wxString& code, const wxString& /*lang*/) -> std::vector<CodeLine> {
    const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    std::vector<CodeLine> lines;
    CodeLine current;
    wxString segment;
    for (const wxUniChar ch : code) {
        if (ch == '\n') {
            if (!segment.empty()) {
                current.push_back({ .text = segment, .colour = fg });
            }
            lines.push_back(std::move(current));
            current = {};
            segment.clear();
        } else {
            segment += ch;
        }
    }
    if (!segment.empty()) {
        current.push_back({ .text = segment, .colour = fg });
    }
    lines.push_back(std::move(current));
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
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

/// DC-backed text measurer with a host-provided cache. Same shape as the
/// one inside `AiChatView` — a single relayout sees only a handful of
/// styles, so a linear scan beats hashing.
class DcMeasurer final : public TextMeasurer {
public:
    DcMeasurer(wxDC& dcRef, wxFont body, wxFont mono, wxFont themed, std::vector<MeasurementEntry>& cache)
    : m_dc(dcRef)
    , m_body(std::move(body))
    , m_mono(std::move(mono))
    , m_themed(std::move(themed))
    , m_cache(cache) {}

    [[nodiscard]] auto width(const wxString& text, const TextStyle& style) const -> int override {
        if (text.empty()) {
            return 0;
        }
        MeasurementEntry& entry = lookup(style);
        if (text == " ") {
            if (entry.spaceWidth < 0) {
                entry.spaceWidth = measure(" ", entry.font);
            }
            return entry.spaceWidth;
        }
        return measure(text, entry.font);
    }

    [[nodiscard]] auto lineHeight(const TextStyle& style) const -> int override {
        MeasurementEntry& entry = lookup(style);
        if (entry.lineHeight < 0) {
            wxCoord textWidth = 0;
            wxCoord textHeight = 0;
            m_dc.GetTextExtent("Ag", &textWidth, &textHeight, nullptr, nullptr, &entry.font);
            entry.lineHeight = textHeight + 4; // a touch of leading
        }
        return entry.lineHeight;
    }

private:
    [[nodiscard]] auto lookup(const TextStyle& style) const -> MeasurementEntry& {
        for (auto& entry : m_cache) {
            if (entry.style == style) {
                return entry;
            }
        }
        m_cache.push_back({ .style = style,
            .font = fontFor(style, m_body, m_mono, m_themed),
            .lineHeight = -1,
            .spaceWidth = -1 });
        return m_cache.back();
    }

    [[nodiscard]] auto measure(const wxString& text, const wxFont& font) const -> int {
        wxCoord textWidth = 0;
        wxCoord textHeight = 0;
        m_dc.GetTextExtent(text, &textWidth, &textHeight, nullptr, nullptr, &font);
        return textWidth;
    }

    wxDC& m_dc;
    wxFont m_body;
    wxFont m_mono;
    wxFont m_themed;
    std::vector<MeasurementEntry>& m_cache;
};

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(MarkdownView, wxScrolled<wxPanel>)
    EVT_PAINT(MarkdownView::onPaint)
    EVT_SIZE(MarkdownView::onSize)
    EVT_MOTION(MarkdownView::onMotion)
    EVT_LEFT_DOWN(MarkdownView::onLeftDown)
    EVT_MOUSEWHEEL(MarkdownView::onMouseWheel)
wxEND_EVENT_TABLE()
// clang-format on

MarkdownView::MarkdownView(wxWindow* parent, const wxWindowID winid)
: wxScrolled(parent, winid, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_imageCache(std::make_unique<MarkdownImageCache>())
, m_highlighter(defaultHighlight) {
    wxScrolled::SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, 1);
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
    resolveFonts();
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const wxString& markdown) {
    if (markdown == m_markdown) {
        return;
    }
    m_markdown = markdown;
    relayout();
    Scroll(0, 0);
    Refresh();
}

void MarkdownView::setHighlighter(CodeFenceHighlighter highlighter) {
    m_highlighter = highlighter ? std::move(highlighter) : defaultHighlight;
    // The cached layout was built with the previous highlighter; force a
    // rebuild so code blocks pick up the new colouring.
    m_document.clear();
    relayout();
    Refresh();
}

void MarkdownView::setImageCache(std::unique_ptr<MarkdownImageCache> cache) {
    m_imageCache = cache ? std::move(cache) : std::make_unique<MarkdownImageCache>();
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
}

void MarkdownView::refreshTheme() {
    resolveFonts();
    m_measurerCache.clear();
    m_document.clear(); // palette / fonts changed — every cached run is stale
    relayout();
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
}

auto MarkdownView::palette() -> MarkdownPalette {
    const wxColour windowBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const wxColour windowText = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour patchRed { kPatchRedR, kPatchRedG, kPatchRedB };
    const wxColour patchGreen { kPatchGreenR, kPatchGreenG, kPatchGreenB };
    return { .text = windowText,
        .link = wxSystemSettings::GetColour(wxSYS_COLOUR_HOTLIGHT),
        .codeBg = blend(windowBg, windowText, kCodeBgBlend),
        .inlineCodeBg = blend(windowBg, windowText, kInlineCodeBgBlend),
        .rule = blend(windowBg, windowText, kRuleBlend),
        .tableHeaderBg = blend(windowBg, windowText, kTableHeaderBgBlend),
        .patchSearchBg = blend(windowBg, patchRed, kPatchHalfBlend),
        .patchReplaceBg = blend(windowBg, patchGreen, kPatchHalfBlend),
        .patchFg = windowText };
}

void MarkdownView::onSize(wxSizeEvent& event) {
    relayout();
    Refresh();
    event.Skip();
}

void MarkdownView::relayout() {
    const int panelWidth = GetClientSize().GetWidth();
    if (panelWidth <= 0) {
        return;
    }
    const int contentWidth = std::max(40, panelWidth - (2 * kPadding));

    wxClientDC clientDc(this);
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

    m_document.setMarkdown(m_markdown, contentWidth, measurer, palette(), m_highlighter, resolveImage);

    const int totalHeight = m_document.height() + (2 * kPadding);
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
        gc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
        gc.DrawRectangle(update);

        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int regionTopDoc = originY + update.y;
        const int regionBottomDoc = regionTopDoc + update.height;

        const MarkdownPalette pal = palette();
        const auto& laid = m_document.laid();
        const int contentLeft = kPadding;
        const int contentTop = kPadding - originY;
        const int contentWidth = std::max(0, size.GetWidth() - (2 * kPadding));

        const int regionTopRel = regionTopDoc - kPadding;
        const int regionBottomRel = regionBottomDoc - kPadding;
        const auto first = std::ranges::lower_bound(
            laid.lines, regionTopRel,
            [](const int height, const int top) { return height < top; },
            [](const PaintLine& line) { return line.y + line.height; }
        );

        PaintRunState runState;
        for (auto it = first; it != laid.lines.end(); ++it) {
            const auto& line = *it;
            if (line.y > regionBottomRel) {
                break;
            }
            const int lineTop = contentTop + line.y;
            fbide::paintLineBackground(gc, line, contentLeft, lineTop, contentWidth, pal);
            fbide::paintLineText(gc, line, contentLeft, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);
        }

        gc.GetGraphicsContext()->Flush();
    }
    paintDc.Blit(update.x, update.y, update.width, update.height, &memoryDc, update.x, update.y);
}

auto MarkdownView::linkAt(const wxPoint& clientPoint) const -> wxString {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int relX = clientPoint.x - kPadding;
    const int relY = clientPoint.y + originY - kPadding;
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

void MarkdownView::onMotion(wxMouseEvent& event) {
    SetCursor(linkAt(event.GetPosition()).empty() ? wxCursor(wxCURSOR_ARROW)
                                                  : wxCursor(wxCURSOR_HAND));
    event.Skip();
}

void MarkdownView::onLeftDown(wxMouseEvent& event) {
    const wxString url = linkAt(event.GetPosition());
    if (url.empty()) {
        event.Skip();
        return;
    }
    // Give parents a chance to intercept the click via the
    // MARKDOWN_LINK_CLICKED event. If nobody handles it (or every
    // handler calls Skip), fall through to launching the browser.
    wxCommandEvent linkEvent(MARKDOWN_LINK_CLICKED, GetId());
    linkEvent.SetEventObject(this);
    linkEvent.SetString(url);
    if (!ProcessWindowEvent(linkEvent)) {
        wxLaunchDefaultBrowser(url);
    }
}

void MarkdownView::onMouseWheel(wxMouseEvent& event) {
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        event.Skip();
        return;
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
