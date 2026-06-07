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
    if (auto* smart = wxDynamicCast(currentSizer(), SmartBoxSizer)) {
        smart->setOptions({ .margin = false });
    }

    const auto lbl = label(m_labelText);
    hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
        m_chkInherit = checkBox(wxEmptyString, {}, ID_CHK_INHERIT);
        if (not m_inheritTooltip.empty()) {
            m_chkInherit->SetToolTip(m_inheritTooltip);
        }
        m_btn = button(wxEmptyString, {}, ID_BTN_COLOR);
        m_btn->SetMinSize(wxSize(110, -1));
    });
    connect(lbl, m_btn);

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

    Fit();
    Layout();
}

auto ColorPicker::getColor() const -> wxColour {
    if (m_chkInherit->IsShown() && m_chkInherit->IsChecked()) {
        return wxNullColour;
    }
    return m_currentColor;
}

void ColorPicker::applyColor(const wxColour& c) {
    // Plain button with a colour swatch as its bitmap and the hex
    // value as its label. The previous version forced the button's
    // background colour to `c`, which fought the native style and
    // rendered inconsistently across platforms / themes.
    m_currentColor = c;
    const auto hex = c.GetAsString(wxC2S_HTML_SYNTAX);
    m_btn->SetBitmap(makeSwatch(c));
    m_btn->SetLabel(hex);
    m_btn->SetToolTip(hex);
}

void ColorPicker::openColourDialog() {
    wxColourData data;
    data.SetColour(m_currentColor);
    if (wxColourDialog dlg(this, &data); dlg.ShowModal() == wxID_OK) {
        applyColor(dlg.GetColourData().GetColour());
    }
}

void ColorPicker::copyHexToClipboard() const {
    if (not m_currentColor.IsOk()) {
        return;
    }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(
            make_unowned<wxTextDataObject>(m_currentColor.GetAsString(wxC2S_HTML_SYNTAX))
        );
        wxTheClipboard->Close();
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
    const int copyHexId = NewControlId();
    menu.Append(chooseId, m_tr.get_or("chooseColor", "Choose color..."));
    menu.Append(copyHexId, m_tr.get_or("copyHex", "Copy hex value"));
    menu.AppendSeparator();

    // "Copy from" mirrors ThemePage's category tree (folders + leaves)
    // from the single shared `settingsCategoryTree()` source, so the two
    // never drift and every palette — including the diff-state colours —
    // is reachable. wx takes ownership of the submenu on AppendSubMenu.
    auto copyMenu = std::make_unique<wxMenu>();
    appendCopyFromNodes(*copyMenu, settingsCategoryTree(), colorMap);
    menu.AppendSubMenu(copyMenu.release(), m_tr.get_or("copyFrom", "Copy from"));

    const int sel = GetPopupMenuSelectionFromUser(menu);
    if (sel == wxID_NONE) {
        return;
    }
    if (sel == chooseId) {
        openColourDialog();
        return;
    }
    if (sel == copyHexId) {
        copyHexToClipboard();
        return;
    }
    if (const auto it = colorMap.find(sel); it != colorMap.end()) {
        applyColor(it->second);
    }
}

void ColorPicker::appendCopyFromNodes(
    wxMenu& parent, const std::vector<SettingsTreeNode>& nodes, std::unordered_map<int, wxColour>& colorMap
) const {
    const wxString fgLabel = m_tr.get_or("foreground", "Foreground");
    const wxString bgLabel = m_tr.get_or("background", "Background");

    // Append one "<label>  <hex>" swatch item bound to `colour`; a no-op
    // for an unset colour so a category that defines only a foreground
    // (or nothing) doesn't sprout blank entries.
    const auto addColourItem = [&](wxMenu& sub, const wxString& text, const wxColour& colour) {
        if (not colour.IsOk()) {
            return;
        }
        const int id = NewControlId();
        const auto hex = colour.GetAsString(wxC2S_HTML_SYNTAX);
        const auto item = make_unowned<wxMenuItem>(&sub, id, text + "  " + hex);
        item->SetBitmap(makeSwatch(colour));
        sub.Append(item);
        colorMap[id] = colour;
    };

    // Submenu label — reuses the same `categories.*` locale keys the
    // ThemePage tree uses (enum-derived for categories, explicit for
    // folders) so every locale's existing translations apply.
    const auto labelFor = [&](const SettingsTreeNode& node) -> wxString {
        if (node.category) {
            const auto sv = getSettingsCategoryLabelKey(*node.category);
            const wxString key = wxString::FromAscii(sv.data(), sv.size());
            return m_tr.get_or("categories." + key, key);
        }
        return m_tr.get_or("categories." + node.labelKey, node.labelKey);
    };

    for (const auto& node : nodes) {
        auto sub = std::make_unique<wxMenu>();

        // A category node contributes its own colour(s) first.
        if (node.category) {
            if (*node.category == SettingsCategory::Changes) {
                // Diff palette — four standalone colours, not an fg/bg pair,
                // so it bypasses `readCategory` (which has no Changes entry).
                addColourItem(*sub, m_tr.get_or("changesBackground", "Background"), m_theme.getChangesBackground());
                addColourItem(*sub, m_tr.get_or("changesAdded", "Added"), m_theme.getChangesAdded());
                addColourItem(*sub, m_tr.get_or("changesModified", "Modified"), m_theme.getChangesModified());
                addColourItem(*sub, m_tr.get_or("changesRemoved", "Removed"), m_theme.getChangesRemoved());
            } else {
                const auto entry = readCategory(m_theme, *node.category);
                addColourItem(*sub, fgLabel, entry.colors.foreground);
                addColourItem(*sub, bgLabel, entry.colors.background);
            }
        }

        // Children follow — folder contents, or a selectable parent's
        // sub-styles (Default / String / Preprocessor) — separated from
        // the node's own colours when both are present.
        if (not node.children.empty()) {
            if (sub->GetMenuItemCount() > 0) {
                sub->AppendSeparator();
            }
            appendCopyFromNodes(*sub, node.children, colorMap);
        }

        // Skip a node that produced nothing rather than show an empty
        // submenu; `unique_ptr` frees it, wx owns the rest after release.
        if (sub->GetMenuItemCount() == 0) {
            continue;
        }
        parent.AppendSubMenu(sub.release(), labelFor(node));
    }
}
