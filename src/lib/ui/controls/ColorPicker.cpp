//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "ColorPicker.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(ColorPicker, wxPanel)
    EVT_CHECKBOX(ColorPicker::ID_CHK_INHERIT, ColorPicker::onInheritToggle)
    EVT_BUTTON  (ColorPicker::ID_BTN_COLOR,   ColorPicker::onButtonClick)
wxEND_EVENT_TABLE()
// clang-format on

ColorPicker::ColorPicker(wxWindow* parent, wxString label, wxString inheritTooltip)
: Layout(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
, m_labelText(std::move(label))
, m_inheritTooltip(std::move(inheritTooltip)) {}

void ColorPicker::create() {
    currentOptions() = { .border = 0 };

    m_lbl = label(m_labelText, {});
    hbox({ .center = true, .border = 0 }, [&] {
        m_chkInherit = make_unowned<wxCheckBox>(currentParent(), ID_CHK_INHERIT, wxEmptyString);
        if (not m_inheritTooltip.empty()) {
            m_chkInherit->SetToolTip(m_inheritTooltip);
        }
        currentSizer()->Add(m_chkInherit, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, defaultBorder());
        m_btn = button(wxString {}, { .proportion = 1, .space = false }, ID_BTN_COLOR);
    });
    connect(m_lbl, m_btn);

    SetSizer(currentSizer());
}

void ColorPicker::setColors(const wxColour& color, const wxColour& defaultColor) {
    m_defaultColor = defaultColor;
    const bool canInherit = defaultColor.IsOk();
    m_chkInherit->Show(canInherit);

    const bool inheriting = canInherit && not color.IsOk();
    m_chkInherit->SetValue(inheriting);
    m_btn->Enable(not inheriting);

    const auto effective = color.IsOk() ? color : defaultColor;
    if (effective.IsOk()) {
        m_btn->SetBackgroundColour(effective);
        m_btn->SetToolTip(effective.GetAsString(wxC2S_HTML_SYNTAX));
        m_btn->Refresh();
    }
    Layout();
}

auto ColorPicker::getColor() const -> wxColour {
    if (m_chkInherit->IsShown() && m_chkInherit->GetValue()) {
        return wxNullColour;
    }
    return m_btn->GetBackgroundColour();
}

void ColorPicker::onInheritToggle(wxCommandEvent&) {
    const bool inheriting = m_chkInherit->GetValue();
    m_btn->Enable(not inheriting);
    if (m_defaultColor.IsOk()) {
        m_btn->SetBackgroundColour(m_defaultColor);
        m_btn->SetToolTip(m_defaultColor.GetAsString(wxC2S_HTML_SYNTAX));
        m_btn->Refresh();
    }
}

void ColorPicker::onButtonClick(wxCommandEvent&) {
    wxColourData data;
    data.SetColour(m_btn->GetBackgroundColour());
    if (wxColourDialog dlg(this, &data); dlg.ShowModal() == wxID_OK) {
        const auto picked = dlg.GetColourData().GetColour();
        m_btn->SetBackgroundColour(picked);
        m_btn->SetToolTip(picked.GetAsString(wxC2S_HTML_SYNTAX));
        m_btn->Refresh();
    }
}
