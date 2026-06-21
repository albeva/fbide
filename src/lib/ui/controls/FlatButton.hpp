//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Flat, owner-drawn push button that renders cleanly on coloured
 * backgrounds — the native `wxButton` can't be reliably tinted on Windows /
 * macOS, so anything sitting on a custom-coloured surface needs this.
 *
 * One configurable **base colour** drives the look; the 1px border, hover,
 * and pressed shades are derived from it, and the text/icon colour
 * auto-contrasts (light text on a dark base, dark on a light one) unless set
 * explicitly. Keyboard focusable (draws a dotted focus ring) and activatable
 * with Space / Enter.
 *
 * Three content modes, inferred from what is set: **text**, **icon + text**,
 * or **icon only**.
 *
 * Emits `wxEVT_BUTTON` with its own id exactly like a native button, so it
 * drops straight into existing `EVT_BUTTON` tables / handlers.
 */
class FlatButton final : public wxControl {
public:
    NO_COPY_AND_MOVE(FlatButton)

    FlatButton(
        wxWindow* parent, wxWindowID id, const wxString& label,
        const wxBitmapBundle& icon = {}, long style = 0
    );
    ~FlatButton() override = default;

    /// Set the base (fill) colour. Border / hover / pressed shades and — when
    /// no explicit text colour is set — the content colour derive from it.
    void setBaseColour(const wxColour& colour);
    /// Override the text + icon colour (otherwise auto-contrasted from base).
    void setTextColour(const wxColour& colour);
    /// Replace the label text.
    void setLabelText(const wxString& label);
    /// Replace the icon.
    void setIcon(const wxBitmapBundle& icon);

    /// Enable or disable the button, repainting so the disabled (greyed) state
    /// shows — the native refresh-on-enable doesn't apply to owner-drawn paint.
    auto Enable(bool enable = true) -> bool override;

protected:
    [[nodiscard]] auto DoGetBestClientSize() const -> wxSize override;

private:
    void onPaint(wxPaintEvent& event);
    void onMouseDown(wxMouseEvent& event);
    void onMouseUp(wxMouseEvent& event);
    void onMouseEnter(wxMouseEvent& event);
    void onMouseLeave(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);
    void onKeyDown(wxKeyEvent& event);
    void onKeyUp(wxKeyEvent& event);
    void onFocusChange(wxFocusEvent& event);

    /// Emit `wxEVT_BUTTON` to the handler chain (propagates to the parent).
    void fire();
    /// Fill colour for the current hover / pressed state.
    [[nodiscard]] auto fillColour() const -> wxColour;
    /// Resolved text + icon colour (explicit override or auto-contrast).
    [[nodiscard]] auto contentColour() const -> wxColour;

    wxBitmapBundle m_icon;
    wxColour m_base;       ///< Fill colour; everything else derives from it.
    wxColour m_textColour; ///< Explicit text/icon colour; invalid → auto-contrast.
    bool m_hovered = false;
    bool m_pressed = false; ///< Mouse- or keyboard-press in progress.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
