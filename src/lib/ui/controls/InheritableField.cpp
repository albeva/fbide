//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "InheritableField.hpp"
using namespace fbide;

namespace {
/// Stable IDs for the widget's three child controls. Kept at file
/// scope so the wxWidgets event-table macros can reference them
/// directly without qualifying.
constexpr int ID_CHK_INHERIT = wxID_HIGHEST + 1;
constexpr int ID_TXT_FIELD = wxID_HIGHEST + 2;
constexpr int ID_BTN_BROWSE = wxID_HIGHEST + 3;
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(InheritableField, wxPanel)
    EVT_CHECKBOX(ID_CHK_INHERIT, InheritableField::onInheritToggle)
    EVT_TEXT    (ID_TXT_FIELD,   InheritableField::onTextChanged)
    EVT_BUTTON  (ID_BTN_BROWSE,  InheritableField::onBrowseClick)
wxEND_EVENT_TABLE()
// clang-format on

InheritableField::InheritableField(wxWindow* parent, Kind kind, wxString labelText, wxString inheritTooltip)
: Layout(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
, m_kind(kind)
, m_labelText(std::move(labelText))
, m_inheritTooltip(std::move(inheritTooltip)) {}

void InheritableField::create() {
    if (auto* smart = wxDynamicCast(currentSizer(), SmartBoxSizer)) {
        smart->setOptions({ .margin = false });
    }

    hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
        m_chkInherit = checkBox(wxEmptyString, {}, ID_CHK_INHERIT);
        if (!m_inheritTooltip.IsEmpty()) {
            m_chkInherit->SetToolTip(m_inheritTooltip);
        }
        const auto lbl = label(m_labelText);
        m_field = textField({ .proportion = 1 }, ID_TXT_FIELD);
        connect(lbl, m_field);
        if (m_kind == Kind::Path) {
            m_browse = button("...", {}, ID_BTN_BROWSE);
        }
    });

    // Default: inheriting, matches the most common state for a freshly-
    // created user configuration.
    m_chkInherit->SetValue(true);
    refreshDisplay();
    SetSizer(currentSizer());
}

void InheritableField::setInherited(bool inherited) {
    m_chkInherit->SetValue(inherited);
    refreshDisplay();
}

auto InheritableField::isInherited() const noexcept -> bool {
    // Canonical Default hides the checkbox — there's no parent to
    // inherit from, so the field always represents an explicit value.
    if (!m_chkInherit->IsShown()) {
        return false;
    }
    return m_chkInherit->GetValue();
}

void InheritableField::setOverrideValue(const wxString& value) {
    m_overrideValue = value;
    refreshDisplay();
}

auto InheritableField::overrideValue() const -> wxString {
    return m_overrideValue;
}

void InheritableField::setResolvedValue(const wxString& value) {
    m_resolvedValue = value;
    refreshDisplay();
}

void InheritableField::setInheritCheckboxVisible(bool visible) {
    m_chkInherit->Show(visible);
    if (!visible) {
        // Hidden → no inheritance possible. Force unticked so
        // refreshDisplay treats the field as an explicit value.
        m_chkInherit->SetValue(false);
    }
    refreshDisplay();
    Layout();
}

void InheritableField::refreshDisplay() {
    const bool inheriting = isInherited();
    m_field->Enable(!inheriting);
    if (m_browse != nullptr) {
        m_browse->Enable(!inheriting);
    }
    // ChangeValue (not SetValue) — SetValue fires wxEVT_TEXT which would
    // loop back into onTextChanged and overwrite m_overrideValue while
    // we're trying to flip into inherited mode.
    m_field->ChangeValue(inheriting ? m_resolvedValue : m_overrideValue);
}

void InheritableField::onInheritToggle(wxCommandEvent& /*event*/) {
    if (!m_chkInherit->GetValue()) {
        // Just unticked (going from inherit → custom). Seed the field
        // with the resolved value so the user has a meaningful starting
        // point per the design doc, rather than an empty field.
        m_overrideValue = m_resolvedValue;
    }
    refreshDisplay();
}

void InheritableField::onTextChanged(wxCommandEvent& /*event*/) {
    if (!isInherited()) {
        m_overrideValue = m_field->GetValue();
    }
}

void InheritableField::onBrowseClick(wxCommandEvent& /*event*/) {
    wxFileDialog dlg(
        this, m_labelText, wxString {}, wxString {},
        "*", wxFD_OPEN | wxFD_FILE_MUST_EXIST
    );
    if (dlg.ShowModal() == wxID_OK) {
        m_overrideValue = dlg.GetPath();
        refreshDisplay();
    }
}
