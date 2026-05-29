//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "config/Value.hpp"
#include "ui/controls/InheritableField.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {
class CompilerConfigCatalog;

/// Compiler settings tab — multi-configuration editor: list on the
/// left, per-config field editor on the right. Edits flow through
/// `CompilerConfigCatalog`. `[compiler]` is snapshotted on `create()`
/// and restored on `cancel()` so the user can roll back any
/// Add / Copy / Remove / Rename / Base / Active / Override.
class CompilerPage final : public Panel {
public:
    NO_COPY_AND_MOVE(CompilerPage)

    explicit CompilerPage(Context& ctx, wxWindow* parent);
    void create() override;
    auto apply() -> bool override;
    void cancel() override;

    /// Move keyboard focus to the path field of the active selection.
    /// Used when the dialog is opened from the startup compiler-missing
    /// prompt so the user can start typing the path immediately.
    void focusCompilerPath();

private:
    /// Locale lookup — resolves keys against the cached
    /// `[dialogs/settings/compiler]` subtree so the full path doesn't
    /// have to be re-resolved on every call.
    [[nodiscard]] auto tr(const wxString& key) const -> wxString {
        return m_locale.get_or(key, key);
    }
    [[nodiscard]] auto catalog() const -> CompilerConfigCatalog&;

    void buildConfigurationsGroup();
    void buildLeftPane();
    void buildRightPane();

    void refreshList();
    void loadSelectedConfig();
    void commitFieldOverrides();
    /// Format the list label for a single configuration — the active
    /// entry gets a localised " (active)" suffix.
    [[nodiscard]] auto formatListLabel(const wxString& slug, const wxString& name) const -> wxString;
    /// Select a slug in the listbox without rebuilding it.
    void selectSlug(const wxString& slug);
    /// Render the active configuration's tree node in bold so the
    /// "active" entry is visually distinct from the currently-selected
    /// one. Called from `refreshList()` — that covers every code path
    /// where the active slug or the tree contents can change.
    void applyActiveBold() const;

    // Event handlers — matched against the event table in the .cpp.
    void onAddClicked(wxCommandEvent& event);
    void onCopyClicked(wxCommandEvent& event);
    void onRemoveClicked(wxCommandEvent& event);
    void onNameChanged(wxCommandEvent& event);
    void onBaseChanged(wxCommandEvent& event);
    void onActiveToggled(wxCommandEvent& event);
    /// Triggered by any of the four `InheritableField`s when the user
    /// toggles its inherit checkbox. Saves the current override value
    /// on tick-on (so an accidental tick can be undone) and restores
    /// from the memory map on tick-off.
    void onInheritToggled(wxCommandEvent& event);
    /// User picked a different node in the configuration tree —
    /// commits the current right-pane state and loads the new
    /// selection. Programmatic re-selections (after Add / Copy / etc.)
    /// no-op when the slug already matches `m_selectedSlug`.
    void onTreeSelChanged(wxTreeEvent& event);

    /// Locale subtree for `[dialogs/settings/compiler]` — see `tr()`.
    const Value& m_locale;

    /// Snapshot of `[compiler]` captured in `create()`; replayed in
    /// `cancel()` to undo every CRUD mutation the user performed
    /// during the dialog session.
    Value m_compilerSnapshot;

    /// Slug currently shown in the right pane; empty if nothing selected.
    wxString m_selectedSlug;

    Unowned<wxTreeCtrl> m_configTree;
    Unowned<wxBitmapButton> m_addButton;
    Unowned<wxBitmapButton> m_copyButton;
    Unowned<wxBitmapButton> m_removeButton;

    /// Reverse maps to bridge between `wxTreeItemId`s (which the tree
    /// gives us in events) and slugs (the catalog key). Both rebuild
    /// inside `refreshList()`.
    std::unordered_map<wxTreeItemIdValue, wxString> m_treeSlugs;
    std::unordered_map<wxString, wxTreeItemId> m_slugItems;

    Unowned<wxStaticText> m_nameLabel;
    Unowned<wxTextCtrl> m_nameField;
    Unowned<wxStaticText> m_baseLabel;
    Unowned<wxChoice> m_baseChoice;
    Unowned<wxCheckBox> m_activeCheckbox;

    Unowned<InheritableField> m_pathField;
    Unowned<InheritableField> m_compileField;
    Unowned<InheritableField> m_runField;
    Unowned<InheritableField> m_terminalField;

    /// Display-name → slug map kept in sync with `m_baseChoice` items.
    std::vector<wxString> m_baseChoiceSlugs;

    /// Per-field memory of the user's last custom override value.
    /// Populated when the user ticks "inherit" so the value can be
    /// restored on a subsequent untick. Cleared whenever a different
    /// configuration is loaded (see `loadSelectedConfig`); naturally
    /// destroyed when the dialog closes.
    std::unordered_map<CompilerField, wxString> m_lastOverrideValues;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
