//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "InheritableField.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(InheritableField, wxPanel)
    EVT_CHECKBOX(InheritableField::ID_CHK_OVERRIDE, InheritableField::onOverrideToggle)
    EVT_TEXT    (InheritableField::ID_TXT_FIELD,    InheritableField::onTextChanged)
    EVT_BUTTON  (InheritableField::ID_BTN_BROWSE,   InheritableField::onBrowseClick)
wxEND_EVENT_TABLE()
// clang-format on

InheritableField::InheritableField(wxWindow* parent, Kind kind, wxString labelText)
: Layout(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
, m_kind(kind)
, m_labelText(std::move(labelText)) {}

void InheritableField::create() {
    if (auto* smart = wxDynamicCast(currentSizer(), SmartBoxSizer)) {
        smart->setOptions({ .margin = false });
    }

    hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
        m_chkOverride = checkBox(wxEmptyString, {}, ID_CHK_OVERRIDE);
        const auto lbl = label(m_labelText);
        m_field = textField({ .proportion = 1 }, ID_TXT_FIELD);
        connect(lbl, m_field);
        if (m_kind == Kind::Path) {
            m_browse = button("...", {}, ID_BTN_BROWSE);
        }
    });

    refreshDisplay();
    SetSizer(currentSizer());
}

void InheritableField::setInherited(bool inherited) {
    m_chkOverride->SetValue(!inherited);
    refreshDisplay();
}

auto InheritableField::isInherited() const noexcept -> bool {
    return !m_chkOverride->GetValue();
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

void InheritableField::refreshDisplay() {
    const bool overriding = m_chkOverride->GetValue();
    m_field->Enable(overriding);
    if (m_browse != nullptr) {
        m_browse->Enable(overriding);
    }
    // ChangeValue (not SetValue) — SetValue fires wxEVT_TEXT which would
    // loop back into onTextChanged and overwrite m_overrideValue with
    // m_resolvedValue when toggling to inherited.
    m_field->ChangeValue(overriding ? m_overrideValue : m_resolvedValue);
}

void InheritableField::onOverrideToggle(wxCommandEvent&) {
    if (m_chkOverride->GetValue()) {
        // Per the design doc: when the user just enabled override,
        // seed the input with the resolved value so they have a
        // meaningful starting point rather than a blank field.
        m_overrideValue = m_resolvedValue;
    }
    refreshDisplay();
}

void InheritableField::onTextChanged(wxCommandEvent&) {
    if (m_chkOverride->GetValue()) {
        m_overrideValue = m_field->GetValue();
    }
}

void InheritableField::onBrowseClick(wxCommandEvent&) {
    wxFileDialog dlg(
        this, m_labelText, wxString {}, wxString {},
        "*", wxFD_OPEN | wxFD_FILE_MUST_EXIST
    );
    if (dlg.ShowModal() == wxID_OK) {
        m_overrideValue = dlg.GetPath();
        refreshDisplay();
    }
}
