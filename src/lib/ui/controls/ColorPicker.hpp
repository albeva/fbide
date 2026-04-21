//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Layout.hpp"

namespace fbide {

/// Color picker control — label, optional "inherit" checkbox, color-swatch button.
/// Self-contained: opens wxColourDialog on click, toggles enabled state when
/// inheriting. Read via getColor(); write via setColors().
class ColorPicker final : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(ColorPicker)

    ColorPicker(wxWindow* parent, wxString label, wxString inheritTooltip = {});

    /// Build the sizer, child controls, and event bindings.
    void create();

    /// Apply value + fallback. When defaultColor is not OK the "inherit"
    /// checkbox is hidden; the picker becomes a plain label + swatch.
    /// When color is not OK but defaultColor is, inherit starts ticked.
    void setColors(const wxColour& color, const wxColour& defaultColor = wxNullColour);

    /// Selected color, or wxNullColour when "inherit" is ticked.
    [[nodiscard]] auto getColor() const -> wxColour;

private:
    enum ControlId : int {
        ID_CHK_INHERIT = wxID_HIGHEST + 1,
        ID_BTN_COLOR
    };

    void onInheritToggle(wxCommandEvent& event);
    void onButtonClick(wxCommandEvent& event);

    wxString              m_labelText;
    wxString              m_inheritTooltip;
    wxColour              m_defaultColor;
    Unowned<wxStaticText> m_lbl;
    Unowned<wxCheckBox>   m_chkInherit;
    Unowned<wxButton>     m_btn;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
