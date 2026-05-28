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

/// Settings widget that mirrors `ColorPicker`'s "[ ] override" semantics
/// for a single inheritable text field — checkbox + label + text input
/// (+ optional Browse button for path fields).
///
/// When the override checkbox is unticked the field is disabled and
/// shows the resolved/inherited value greyed; ticking it enables
/// editing and seeds the input with the resolved value so the user
/// doesn't start from a blank slate. Used by the compiler configuration
/// settings panel to round-trip per-field overrides via the catalog.
class InheritableField final : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(InheritableField)

    enum class Kind : std::uint8_t { Text, Path };

    InheritableField(wxWindow* parent, Kind kind, wxString labelText);

    /// Build the layout. Call once after construction (mirrors
    /// `ColorPicker`'s two-phase init).
    void create();

    /// Toggle the inherit state — checkbox + field enablement + display
    /// update. Does not mutate the stored override value.
    void setInherited(bool inherited);

    /// True when the override checkbox is unticked.
    [[nodiscard]] auto isInherited() const noexcept -> bool;

    /// Set the value held for the "override on" state. Visible
    /// immediately if the checkbox is ticked.
    void setOverrideValue(const wxString& value);

    /// The value to persist when the checkbox is ticked.
    [[nodiscard]] auto overrideValue() const -> wxString;

    /// Set the resolved/inherited value — what gets shown greyed in the
    /// disabled field while the checkbox is unticked.
    void setResolvedValue(const wxString& value);

private:
    enum ControlId : int {
        ID_CHK_OVERRIDE = wxID_HIGHEST + 1,
        ID_TXT_FIELD,
        ID_BTN_BROWSE,
    };

    void onOverrideToggle(wxCommandEvent& event);
    void onTextChanged(wxCommandEvent& event);
    void onBrowseClick(wxCommandEvent& event);

    /// Update field/browse enable state and reflect either the override
    /// or resolved value depending on the checkbox.
    void refreshDisplay();

    Kind m_kind;
    wxString m_labelText;
    wxString m_resolvedValue; ///< Displayed (greyed) when inheriting.
    wxString m_overrideValue; ///< Persisted when overriding.
    Unowned<wxCheckBox> m_chkOverride;
    Unowned<wxTextCtrl> m_field;
    Unowned<wxButton> m_browse; ///< Only constructed for `Kind::Path`.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
