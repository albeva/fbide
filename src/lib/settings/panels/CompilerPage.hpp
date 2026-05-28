//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "ui/controls/InheritableField.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {
class CompilerConfigCatalog;

/// Compiler settings tab — multiple-configuration editor (list on the
/// left, per-config field editor on the right) plus a CHM help-file
/// picker. Each catalog mutation (Add / Copy / Remove / rename / base /
/// active / field overrides) flows through `CompilerConfigCatalog`, so
/// the toolbar combobox stays in sync as the user edits.
class CompilerPage final : public Panel {
public:
    NO_COPY_AND_MOVE(CompilerPage)

    explicit CompilerPage(Context& ctx, wxWindow* parent);
    void create() override;
    void apply() override;

    /// Move keyboard focus to the path field of the active selection.
    /// Used when the dialog is opened from the startup compiler-missing
    /// prompt so the user can start typing the path immediately.
    void focusCompilerPath();

private:
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }
    [[nodiscard]] auto catalog() const -> CompilerConfigCatalog&;

    void buildConfigurationsGroup();
    void buildLeftPane();
    void buildRightPane();
    void buildHelpGroup();

    void refreshList();
    void loadSelectedConfig();
    void commitFieldOverrides();

    void onListSelected();
    void onAddClicked();
    void onCopyClicked();
    void onRemoveClicked();
    void onNameChanged();
    void onBaseChanged();
    void onActiveToggled();

    /// Slug currently shown in the right pane; empty if nothing selected.
    wxString m_selectedSlug;

    Unowned<wxListBox> m_configList;
    Unowned<wxButton> m_addButton;
    Unowned<wxButton> m_copyButton;
    Unowned<wxButton> m_removeButton;

    Unowned<wxTextCtrl> m_nameField;
    Unowned<wxStaticText> m_slugLabel;
    Unowned<wxChoice> m_baseChoice;
    Unowned<wxCheckBox> m_activeCheckbox;

    Unowned<InheritableField> m_pathField;
    Unowned<InheritableField> m_compileField;
    Unowned<InheritableField> m_runField;
    Unowned<InheritableField> m_terminalField;

    /// Display-name → slug map kept in sync with `m_baseChoice` items.
    std::vector<wxString> m_baseChoiceSlugs;

    /// CHM help file path — relocated from the legacy single-config layout.
    wxString m_helpFile;
};

} // namespace fbide
