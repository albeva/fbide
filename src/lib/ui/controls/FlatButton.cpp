//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FlatButton.hpp"
using namespace fbide;

namespace {
// Inner paddings (logical px, scaled via FromDIP). Corners are square — the
// bar is going for simplicity, not chrome.
constexpr int kPadX = 10;
constexpr int kPadY = 5;
constexpr int kIconTextGap = 6;
constexpr int kFocusInset = 3;

// Rec. 601 luminance weights for the light/dark content-colour decision.
constexpr double kLumaR = 0.299;
constexpr double kLumaG = 0.587;
constexpr double kLumaB = 0.114;
constexpr double kByteMax = 255.0;
constexpr double kDarkThreshold = 0.5;

// Blend factors for the shades derived from the base colour.
constexpr double kBorderMix = 0.25;  ///< Base → contrast, for the 1px border.
constexpr double kHoverMix = 0.10;   ///< Base → content, on hover.
constexpr double kPressedMix = 0.22; ///< Base → content, while pressed.

// Auto-contrast content colours, used when no explicit text colour is set.
const wxColour kAutoLightText { 240, 240, 240 };
const wxColour kAutoDarkText { 28, 28, 28 };

/// Component-wise linear blend, `factor` in [0,1] from `from` toward `to`.
auto blend(const wxColour& from, const wxColour& to, const double factor) -> wxColour {
    const auto mix = [factor](const unsigned char src, const unsigned char dst) {
        return static_cast<unsigned char>(src + ((dst - src) * factor));
    };
    return { mix(from.Red(), to.Red()), mix(from.Green(), to.Green()), mix(from.Blue(), to.Blue()) };
}

/// Perceived luminance in [0,1].
auto luminance(const wxColour& colour) -> double {
    return ((kLumaR * colour.Red()) + (kLumaG * colour.Green()) + (kLumaB * colour.Blue())) / kByteMax;
}

auto isDark(const wxColour& colour) -> bool {
    return luminance(colour) < kDarkThreshold;
}

/// Direct2D-backed graphics context on Windows (crisper than GDI+ for
/// wxGCDC), falling back to the default renderer elsewhere — mirrors the
/// project's MarkdownView paint idiom.
template<typename DcT>
auto makeGraphicsContext(DcT& target) -> wxGraphicsContext* {
    wxGraphicsRenderer* renderer = nullptr; // CreateContext is non-const
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
wxBEGIN_EVENT_TABLE(FlatButton, wxControl)
    EVT_PAINT             (FlatButton::onPaint)
    EVT_LEFT_DOWN         (FlatButton::onMouseDown)
    EVT_LEFT_UP           (FlatButton::onMouseUp)
    EVT_ENTER_WINDOW      (FlatButton::onMouseEnter)
    EVT_LEAVE_WINDOW      (FlatButton::onMouseLeave)
    EVT_MOUSE_CAPTURE_LOST(FlatButton::onCaptureLost)
    EVT_KEY_DOWN          (FlatButton::onKeyDown)
    EVT_KEY_UP            (FlatButton::onKeyUp)
    EVT_SET_FOCUS         (FlatButton::onFocusChange)
    EVT_KILL_FOCUS        (FlatButton::onFocusChange)
wxEND_EVENT_TABLE()
// clang-format on

FlatButton::FlatButton(wxWindow* parent, const wxWindowID id, const wxString& label, const wxBitmapBundle& icon, const long style)
: wxControl(parent, id, wxDefaultPosition, wxDefaultSize, style | wxBORDER_NONE)
, m_icon(icon)
, m_base(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)) {
    // Custom paint with no background erase — flicker-free.
    wxControl::SetBackgroundStyle(wxBG_STYLE_PAINT);
    wxControl::SetLabel(label);
}

void FlatButton::setBaseColour(const wxColour& colour) {
    m_base = colour;
    Refresh();
}

void FlatButton::setTextColour(const wxColour& colour) {
    m_textColour = colour;
    Refresh();
}

void FlatButton::setLabelText(const wxString& label) {
    SetLabel(label);
    InvalidateBestSize();
    Refresh();
}

void FlatButton::setIcon(const wxBitmapBundle& icon) {
    m_icon = icon;
    InvalidateBestSize();
    Refresh();
}

auto FlatButton::contentColour() const -> wxColour {
    if (m_textColour.IsOk()) {
        return m_textColour;
    }
    return isDark(m_base) ? kAutoLightText : kAutoDarkText;
}

auto FlatButton::fillColour() const -> wxColour {
    // Nudge toward the content colour for state feedback — visible on both
    // light and dark bases (a fixed lighten/darken washes out at the extremes).
    if (m_pressed) {
        return blend(m_base, contentColour(), kPressedMix);
    }
    if (m_hovered) {
        return blend(m_base, contentColour(), kHoverMix);
    }
    return m_base;
}

auto FlatButton::DoGetBestClientSize() const -> wxSize {
    const wxString label = GetLabel();
    const wxSize text = label.empty() ? wxSize(0, 0) : GetTextExtent(label);
    const wxSize icon = m_icon.IsOk() ? m_icon.GetDefaultSize() : wxSize(0, 0);
    const int gap = (text.x > 0 && icon.x > 0) ? FromDIP(kIconTextGap) : 0;
    const int width = (FromDIP(kPadX) * 2) + icon.x + gap + text.x;
    const int height = (FromDIP(kPadY) * 2) + std::max(text.y, icon.y);
    return { width, height };
}

void FlatButton::onPaint(wxPaintEvent& /*event*/) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) {
        return;
    }

    wxGCDC gc;
    gc.SetGraphicsContext(makeGraphicsContext(dc));

    const wxColour fg = contentColour();
    const wxColour border = blend(m_base, isDark(m_base) ? *wxWHITE : *wxBLACK, kBorderMix);

    // Body: flat fill with a 1px square border.
    gc.SetBrush(wxBrush(fillColour()));
    gc.SetPen(wxPen(border, 1));
    gc.DrawRectangle(0, 0, size.x, size.y);

    // Content (icon + text), centred as a group. The icon bitmap carries its
    // own scale factor, so DrawBitmap places it at the right logical size.
    const wxSize iconSize = m_icon.IsOk() ? m_icon.GetDefaultSize() : wxSize(0, 0);
    const wxString label = GetLabel();
    gc.SetFont(GetFont());
    gc.SetTextForeground(fg);
    wxCoord textW = 0;
    wxCoord textH = 0;
    if (!label.empty()) {
        gc.GetTextExtent(label, &textW, &textH);
    }
    const int gap = (textW > 0 && iconSize.x > 0) ? FromDIP(kIconTextGap) : 0;
    const int contentW = iconSize.x + gap + textW;
    int penX = (size.x - contentW) / 2;
    if (m_icon.IsOk()) {
        gc.DrawBitmap(m_icon.GetBitmapFor(this), penX, (size.y - iconSize.y) / 2, true);
        penX += iconSize.x + gap;
    }
    if (!label.empty()) {
        gc.DrawText(label, penX, (size.y - textH) / 2);
    }

    // Keyboard focus ring — dotted, inset from the border.
    if (HasFocus()) {
        const int inset = FromDIP(kFocusInset);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.SetPen(wxPen(fg, 1, wxPENSTYLE_DOT));
        gc.DrawRectangle(inset, inset, size.x - (2 * inset), size.y - (2 * inset));
    }

    gc.GetGraphicsContext()->Flush();
}

void FlatButton::fire() {
    wxCommandEvent evt(wxEVT_BUTTON, GetId());
    evt.SetEventObject(this);
    ProcessWindowEvent(evt);
}

void FlatButton::onMouseDown(wxMouseEvent& /*event*/) {
    if (!IsEnabled()) {
        return;
    }
    SetFocus();
    m_pressed = true;
    CaptureMouse();
    Refresh();
}

void FlatButton::onMouseUp(wxMouseEvent& event) {
    if (HasCapture()) {
        ReleaseMouse();
    }
    if (!m_pressed) {
        return;
    }
    m_pressed = false;
    Refresh();
    // Fire only when released over the button (drag-off cancels, as expected).
    if (wxRect(GetClientSize()).Contains(event.GetPosition())) {
        fire();
    }
}

void FlatButton::onMouseEnter(wxMouseEvent& /*event*/) {
    m_hovered = true;
    Refresh();
}

void FlatButton::onMouseLeave(wxMouseEvent& /*event*/) {
    m_hovered = false;
    Refresh();
}

void FlatButton::onCaptureLost(wxMouseCaptureLostEvent& /*event*/) {
    m_pressed = false;
    Refresh();
}

void FlatButton::onKeyDown(wxKeyEvent& event) {
    switch (event.GetKeyCode()) {
    case WXK_SPACE:
        m_pressed = true;
        Refresh();
        break;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
        fire();
        break;
    default:
        event.Skip(); // leave Tab / arrows for dialog navigation
    }
}

void FlatButton::onKeyUp(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_SPACE && m_pressed) {
        m_pressed = false;
        Refresh();
        fire();
        return;
    }
    event.Skip();
}

void FlatButton::onFocusChange(wxFocusEvent& event) {
    Refresh(); // repaint the focus ring on gain / loss
    event.Skip();
}
