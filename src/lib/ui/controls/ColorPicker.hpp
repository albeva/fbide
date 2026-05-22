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
class Theme;
class Value;

/// Color picker control — label, optional "inherit" checkbox, color-swatch button.
/// Clicking the button pops a menu: "Choose color..." opens the color dialog;
/// "Copy from" lists every settings category with foreground/background preview.
class ColorPicker final : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(ColorPicker)

    /// Construct without populating widgets; `create()` builds the UI.
    ColorPicker(wxWindow* parent, const Theme& theme, const Value& tr,
        wxString label, wxString inheritTooltip = {});

    /// Build the sizer, child controls, and event bindings.
    void create();

    /// Apply value + fallback. When defaultColor is not OK the "inherit"
    /// checkbox is hidden; the picker becomes a plain label + swatch.
    /// When color is not OK but defaultColor is, inherit starts ticked.
    void setColors(const wxColour& color, const wxColour& defaultColor = wxNullColour);

    /// Selected color, or `wxNullColour` when "inherit" is ticked.
    [[nodiscard]] auto getColor() const -> wxColour;

private:
    /// Stable wx IDs for this picker's interactive controls.
    enum ControlId : int {
        ID_CHK_INHERIT = wxID_HIGHEST + 1, ///< Inherit-from-default checkbox.
        ID_BTN_COLOR                       ///< Color swatch button.
    };

    /// Inherit checkbox toggled — switch between custom/default colour.
    void onInheritToggle(wxCommandEvent& event);
    /// Swatch button clicked — open the popup menu.
    void onButtonClick(wxCommandEvent& event);

    /// Update the swatch button to display `c`.
    void applyColor(const wxColour& c);
    /// Open the platform colour dialog and apply the chosen colour.
    void openColourDialog();

    const Theme& m_theme;             ///< Active theme — source for "Copy from" entries.
    const Value& m_tr;                ///< Locale subtree for translations.
    wxString m_labelText;             ///< Label text shown next to the swatch.
    wxString m_inheritTooltip;        ///< Tooltip on the inherit checkbox.
    wxColour m_defaultColor;          ///< Fallback colour used when "inherit" is ticked.
    Unowned<wxStaticText> m_lbl;      ///< Label widget.
    Unowned<wxCheckBox> m_chkInherit; ///< Inherit checkbox.
    Unowned<wxButton> m_btn;          ///< Swatch button.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
