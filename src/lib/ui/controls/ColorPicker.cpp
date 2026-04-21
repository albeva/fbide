//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "ColorPicker.hpp"
#include "config/Theme.hpp"
#include "config/Value.hpp"
#include "settings/panels/SettingsCategory.hpp"
using namespace fbide;

namespace {

/// Lowercase first char — locale keys use lowerFirst(#Name).
auto lowerFirst(const std::string_view name) -> wxString {
    wxString out = wxString::FromAscii(name.data(), name.size());
    if (not out.empty()) {
        out[0] = wxTolower(out[0]);
    }
    return out;
}

/// 16×16 swatch filled with `c` and outlined, for menu item bitmaps.
auto makeSwatch(const wxColour& c) -> wxBitmap {
    constexpr int size = 16;
    wxBitmap bmp(size, size);
    wxMemoryDC dc(bmp);
    dc.SetBackground(wxBrush(c));
    dc.Clear();
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(0, 0, size, size);
    return bmp;
}

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(ColorPicker, wxPanel)
    EVT_CHECKBOX(ColorPicker::ID_CHK_INHERIT, ColorPicker::onInheritToggle)
    EVT_BUTTON  (ColorPicker::ID_BTN_COLOR,   ColorPicker::onButtonClick)
wxEND_EVENT_TABLE()
// clang-format on

ColorPicker::ColorPicker(wxWindow* parent, const Theme& theme, const Value& tr,
    wxString label, wxString inheritTooltip)
: Layout(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
, m_theme(theme)
, m_tr(tr)
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
        applyColor(effective);
    }
    Layout();
}

auto ColorPicker::getColor() const -> wxColour {
    if (m_chkInherit->IsShown() && m_chkInherit->IsChecked()) {
        return wxNullColour;
    }
    return m_btn->GetBackgroundColour();
}

void ColorPicker::applyColor(const wxColour& c) {
    m_btn->SetBackgroundColour(c);
    m_btn->SetToolTip(c.GetAsString(wxC2S_HTML_SYNTAX));
    m_btn->Refresh();
}

void ColorPicker::openColourDialog() {
    wxColourData data;
    data.SetColour(m_btn->GetBackgroundColour());
    if (wxColourDialog dlg(this, &data); dlg.ShowModal() == wxID_OK) {
        applyColor(dlg.GetColourData().GetColour());
    }
}

void ColorPicker::onInheritToggle(wxCommandEvent&) {
    const bool inheriting = m_chkInherit->GetValue();
    m_btn->Enable(not inheriting);
    if (m_defaultColor.IsOk()) {
        applyColor(m_defaultColor);
    }
}

void ColorPicker::onButtonClick(wxCommandEvent&) {
    wxMenu menu;
    std::unordered_map<int, wxColour> colorMap;

    const int chooseId = NewControlId();
    menu.Append(chooseId, m_tr.get_or("chooseColor", "Choose color..."));
    menu.AppendSeparator();

    auto* copyMenu = new wxMenu;
    const wxString fgLabel = m_tr.get_or("foreground", "Foreground");
    const wxString bgLabel = m_tr.get_or("background", "Background");

    const auto addCategory = [&](const SettingsCategory cat, const std::string_view rawName) {
        const auto entry = readCategory(m_theme, cat);
        const bool hasFg = entry.colors.foreground.IsOk();
        const bool hasBg = entry.colors.background.IsOk();
        if (not hasFg and not hasBg) {
            return;
        }

        const auto sub = make_unowned<wxMenu>();
        if (hasFg) {
            const int id = NewControlId();
            const auto hex = entry.colors.foreground.GetAsString(wxC2S_HTML_SYNTAX);
            const auto item = make_unowned<wxMenuItem>(sub, id, fgLabel + "  " + hex);
            item->SetBitmap(makeSwatch(entry.colors.foreground));
            sub->Append(item);
            colorMap[id] = entry.colors.foreground;
        }
        if (hasBg) {
            const int id = NewControlId();
            const auto hex = entry.colors.background.GetAsString(wxC2S_HTML_SYNTAX);
            const auto item = make_unowned<wxMenuItem>(sub, id, bgLabel + "  " + hex);
            item->SetBitmap(makeSwatch(entry.colors.background));
            sub->Append(item);
            colorMap[id] = entry.colors.background;
        }
        const auto keyName = lowerFirst(rawName);
        const auto label = m_tr.get_or("categories." + keyName, keyName);
        copyMenu->AppendSubMenu(sub, label);
    };

    // clang-format off
    #define ADD_CAT(NAME, ...) addCategory(SettingsCategory::NAME, #NAME);
        DEFINE_SETTINGS_CATEGORY(ADD_CAT)
    #undef ADD_CAT
    // clang-format on

    menu.AppendSubMenu(copyMenu, m_tr.get_or("copyFrom", "Copy from"));

    const int sel = GetPopupMenuSelectionFromUser(menu);
    if (sel == wxID_NONE) {
        return;
    }
    if (sel == chooseId) {
        openColourDialog();
        return;
    }
    if (const auto it = colorMap.find(sel); it != colorMap.end()) {
        applyColor(it->second);
    }
}
