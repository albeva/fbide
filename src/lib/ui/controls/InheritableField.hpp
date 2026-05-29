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

/// Fired after the user toggles the inherit checkbox on an
/// `InheritableField`. The `int` payload carries the new state
/// (`1` = inheriting, `0` = custom override). `GetEventObject()`
/// points back at the firing widget. Used by `CompilerPage` to
/// remember the user's last override value so an accidental tick
/// can be undone.
wxDECLARE_EVENT(EVT_INHERIT_TOGGLED, wxCommandEvent);

/// Settings widget mirroring `ColorPicker`'s inherit-tickbox semantics
/// for a single text field — checkbox + label + text input
/// (+ optional Browse button for path fields).
///
/// Ticked = inherit (field disabled, shows the resolved/inherited value
/// greyed); unticked = custom (field enabled, user supplies an
/// override value). `setInheritCheckboxVisible(false)` hides the tickbox
/// entirely — the field then behaves as a plain editable input, used
/// for the canonical Default configuration where there is no base to
/// inherit from.
class InheritableField final : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(InheritableField)

    enum class Kind : std::uint8_t { Text,
        Path };

    InheritableField(wxWindow* parent, Kind kind, wxString labelText, wxString inheritTooltip = {});

    /// Build the layout. Call once after construction (mirrors
    /// `ColorPicker`'s two-phase init).
    void create();

    /// Toggle the inherit state — checkbox + field enablement + display
    /// update. Does not mutate the stored override value.
    void setInherited(bool inherited);

    /// True when the inherit checkbox is ticked (or false when the
    /// checkbox is hidden via `setInheritCheckboxVisible(false)`).
    [[nodiscard]] auto isInherited() const noexcept -> bool;

    /// Set the value to display + persist when the inherit checkbox is
    /// unticked (override mode).
    void setOverrideValue(const wxString& value);

    /// Current value to persist when the inherit checkbox is unticked.
    [[nodiscard]] auto overrideValue() const -> wxString;

    /// Set the resolved/inherited value — what gets shown greyed in the
    /// disabled field while the inherit checkbox is ticked.
    void setResolvedValue(const wxString& value);

    /// Show or hide the inherit checkbox. When hidden the field behaves
    /// as a plain editable input — `isInherited()` always returns false.
    ///
    /// Hiding force-unticks the checkbox and the prior tick state is
    /// **not** restored on a subsequent show. Callers that flip both
    /// ways (e.g. CompilerPage switching between canonical and user
    /// rows) must re-apply `setInherited(...)` after the show.
    void setInheritCheckboxVisible(bool visible);

private:
    void onInheritToggle(wxCommandEvent& event);
    void onTextChanged(wxCommandEvent& event);
    void onBrowseClick(wxCommandEvent& event);

    /// Update field/browse enable state and reflect either the override
    /// or resolved value depending on the checkbox.
    void refreshDisplay();

    Kind m_kind;
    wxString m_labelText;
    wxString m_inheritTooltip;
    wxString m_resolvedValue; ///< Displayed (greyed) when inheriting.
    wxString m_overrideValue; ///< Persisted when overriding.
    Unowned<wxCheckBox> m_chkInherit;
    Unowned<wxTextCtrl> m_field;
    Unowned<wxButton> m_browse; ///< Only constructed for `Kind::Path`.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
