//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownView.hpp"
using namespace fbide;
using namespace fbide::ai;
using namespace fbide::markdown;

// NOLINTNEXTLINE(cert-err58-cpp, bugprone-throwing-static-initialization)
wxDEFINE_EVENT(fbide::markdown::MARKDOWN_LINK_CLICKED, wxCommandEvent);

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

MarkdownView::MarkdownView(wxWindow* parent, const wxWindowID winid)
: wxScrolled(parent, winid, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxCLIP_CHILDREN)
, m_imageCache(std::make_unique<MarkdownImageCache>())
, m_highlighter(defaultHighlight) {
    wxScrolled::SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, 1);
    installImageCacheListener();
    resolveFonts();
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const wxString& markdown) {
    if (markdown == m_markdown) {
        return;
    }
    // Content change invalidates any positions the selection held.
    m_selection.clear();
    m_markdown = markdown;
    relayout();
    Scroll(0, 0);
    Refresh();
}

void MarkdownView::setHighlighter(CodeFenceHighlighter highlighter) {
    m_highlighter = highlighter ? std::move(highlighter) : defaultHighlight;
    // The cached layout was built with the previous highlighter; force a
    // rebuild so code blocks pick up the new colouring. Any cached
    // selection positions belong to the old code-run layout.
    m_selection.clear();
    m_document.clear();
    relayout();
    Refresh();
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
    relayout();
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

void MarkdownView::relayout() {
    const int panelWidth = GetClientSize().GetWidth();
    if (panelWidth <= 0) {
        return;
    }
    const int contentWidth = std::max(40, panelWidth - (2 * kPadding));

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
        constexpr int contentLeft = kPadding;
        const int contentTop = kPadding - originY;
        const int contentWidth = std::max(0, size.GetWidth() - (2 * kPadding));

        const int regionTopRel = regionTopDoc - kPadding;
        const int regionBottomRel = regionBottomDoc - kPadding;
        const auto first = std::ranges::lower_bound(
            laid.lines, regionTopRel,
            [](const int height, const int top) { return height < top; },
            [](const PaintLine& line) { return line.y + line.height; }
        );

        // One measurer reused across the line loop — `paintSelectionHighlight`
        // needs per-character widths to compute the highlight rect, and a
        // fresh DcMeasurer + the shared cache is cheap to build.
        const DcMeasurer measurer(memoryDc, m_bodyFont, m_monoFont, m_themedFont, m_measurerCache);
        // Translucent selection so code / patch / table backgrounds and
        // inline images bleed through — drawing solid would obscure them
        // and the band would read as an opaque blue strip.
        const wxColour sysHighlight = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
        constexpr unsigned char kHighlightAlpha = 100;
        const wxColour highlightColour(sysHighlight.Red(), sysHighlight.Green(), sysHighlight.Blue(), kHighlightAlpha);

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
            markdown::paintSelectionHighlight(gc, line, lineIdx, contentLeft, lineTop, contentWidth, nextLineY, m_selection, highlightColour, measurer);
            markdown::paintLineText(gc, line, contentLeft, lineTop, m_bodyFont, m_monoFont, m_themedFont, runState);
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

auto MarkdownView::hitTest(const wxPoint& clientPoint) -> SelectionPosition {
    const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
    const int relX = clientPoint.x - kPadding;
    const int relY = clientPoint.y + originY - kPadding;
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
    if (m_dragSelecting && event.LeftIsDown()) {
        m_selection.caret = hitTest(event.GetPosition());
        Refresh();
        event.Skip();
        return;
    }
    // Idle cursor: hand over links, I-beam over text content, arrow
    // otherwise. Cheap to recompute each motion — `linkAt` is a quick
    // scan and the line lookup is short.
    const bool overLink = !linkAt(event.GetPosition()).empty();
    if (overLink) {
        SetCursor(wxCursor(wxCURSOR_HAND));
    } else {
        // I-beam when the point lands on a text-bearing line of the
        // laid document; arrow when it's in the empty area.
        const int originY = CalcUnscrolledPosition(wxPoint(0, 0)).y;
        const int relY = event.GetPosition().y + originY - kPadding;
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
    if (event.ControlDown() || event.CmdDown()) {
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
