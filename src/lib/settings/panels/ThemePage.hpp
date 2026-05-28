//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "SettingsCategory.hpp"
#include "config/Theme.hpp"
#include "config/Value.hpp"
#include "ui/controls/ColorPicker.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {

/// Theme settings tab — category list on the left, style editor on the right.
class ThemePage final : public Panel {
public:
    NO_COPY_AND_MOVE(ThemePage)

    /// Construct without populating widgets; `create()` builds the UI.
    ThemePage(Context& ctx, wxWindow* parent);

    /// True when the active selection is the synthetic "New theme..." slot.
    [[nodiscard]] auto isUnsavedNewTheme() const -> bool { return m_themeChoice->GetSelection() == 0; }

    /// Build the panel widgets.
    void create() override;
    /// Commit edits back into `ConfigManager`. Always succeeds.
    auto apply() -> bool override;

    /// Enumerate every fixed-width system font — used only by this panel.
    [[nodiscard]] static auto getAllFixedWidthFonts() -> std::vector<wxString>;

    /// Static layout descriptor for the category tree. A node is
    /// either bound to a `SettingsCategory` (clickable to edit
    /// colours) or it's a folder with a hand-written `labelKey`
    /// (`keywords`, `margins`, etc. — those don't map 1:1 to any
    /// category). For category-bound nodes the locale key is
    /// derived from `getSettingsCategoryLabelKey`, so the tree
    /// always matches what ColorPicker's "Copy from" submenu
    /// shows. The label falls back to the locale key itself when
    /// the lookup is missing.
    struct TreeNode {
        std::optional<SettingsCategory> category;
        wxString labelKey; ///< Folder-only; empty when `category` is set.
        std::vector<TreeNode> children;

        /// Category-bound node — label key derived from the enum.
        // NOLINTNEXTLINE(google-explicit-constructor)
        TreeNode(const SettingsCategory cat, std::vector<TreeNode> kids = {})
        : category(cat)
        , children(std::move(kids)) {}

        /// Folder node — no category, explicit locale key.
        // NOLINTNEXTLINE(google-explicit-constructor)
        TreeNode(wxString key, std::vector<TreeNode> kids = {})
        : labelKey(std::move(key))
        , children(std::move(kids)) {}
    };

private:
    /// Locale lookup with optional default — wraps `m_tr.get_or`.
    auto tr(const wxString& path, const wxString& def = wxEmptyString) const -> wxString;

    /// Build the theme-picker row + Save button.
    void createTopRow();
    /// Build the category list (left half).
    void createCategoryList();
    /// Add node to the tree
    void addTreeNode(const wxTreeItemId parent, const std::vector<TreeNode>& nodes);
    /// Build the left half of the page.
    void createLeftPanel();
    /// Build the right half (style editor).
    void createRightPanel();

    /// Stable wx IDs for this panel's interactive controls.
    enum ControlId : int {
        ID_THEME_CHOICE = wxID_HIGHEST + 1, ///< Theme dropdown.
        ID_SAVE_THEME,                      ///< Save-as-new-theme button.
        ID_CATEGORY_TREE                    ///< Category tree control.
    };

    /// Category tree selection changing — veto when target is a folder.
    void onCategorySelChanging(wxTreeEvent& event);
    /// Category tree selection changed — refresh editor on the right.
    void onCategorySelChanged(wxTreeEvent& event);
    /// Theme dropdown selection changed — load the new theme.
    void onSelectTheme(wxCommandEvent& event);
    /// Save button clicked — prompt for a name and save the working theme.
    void onSaveTheme(wxCommandEvent& event);

    /// Show the name dialog and save the working theme as a new file.
    void saveNewTheme(bool setActive);
    /// Pull the currently selected category's entry into the editor widgets.
    void loadCategory();
    /// Push the editor widgets back into the working theme's category entry.
    void saveCategory();
    /// Load the four diff-state pickers from the working theme.
    void loadChangesCategory();
    /// Push the four diff-state pickers back into the working theme.
    void saveChangesCategory();
    /// Enable/disable category-specific widgets (e.g. font fields).
    void applyCapability();
    /// Persist the active theme path back to config.
    void syncActiveThemeConfig();

    Unowned<wxTreeCtrl> m_typeTree; ///< Theme category tree.
    /// Maps each selectable tree-item id to its `SettingsCategory`. Folder
    /// nodes (Keywords / Margins / Brace / Asm) have no entry — absence
    /// signals "non-selectable" without paying for a per-item heap object.
    std::unordered_map<wxTreeItemIdValue, SettingsCategory> m_treeCategories;
    Unowned<wxChoice> m_themeChoice;          ///< Theme picker dropdown.
    Unowned<ColorPicker> m_fgPicker;          ///< Foreground color picker.
    Unowned<ColorPicker> m_bgPicker;          ///< Background color picker.
    Unowned<ColorPicker> m_separatorPicker;   ///< Separator color picker.
    Unowned<wxChoice> m_fontChoice;           ///< Editor font dropdown.
    Unowned<wxStaticText> m_fontOptionsLabel; ///< Bold/italic/underline label.
    Unowned<wxCheckBox> m_chkBold;            ///< Bold toggle.
    Unowned<wxCheckBox> m_chkItalic;          ///< Italic toggle.
    Unowned<wxCheckBox> m_chkUnderline;       ///< Underline toggle.
    Unowned<wxSpinCtrl> m_spinFontSize;       ///< Font size spinner.
    Unowned<wxStaticText> m_lblFont;          ///< Font label.
    Unowned<wxStaticText> m_lblFontSize;      ///< Font-size label.

    // Diff-state pickers — shown only when the "Changes" tree node is
    // selected. The standard fg/bg pickers are hidden in that case.
    Unowned<ColorPicker> m_changesAddedPicker;
    Unowned<ColorPicker> m_changesModifiedPicker;
    Unowned<ColorPicker> m_changesRemovedPicker;
    Unowned<ColorPicker> m_changesBackgroundPicker;

    wxStaticText* m_themeLabel = nullptr; ///< Right-pane group box.

    /// Index offset between the theme dropdown selection and `m_themeFiles`
    /// (slot 0 is the synthetic "New theme..." entry).
    static constexpr int FILE_OFFSET = 1;
    Theme m_theme {};                   ///< Working copy of the theme.
    wxString m_activeTheme;             ///< Path to the active theme on load.
    std::vector<wxString> m_themeFiles; ///< Paths of every available theme.

    int m_selectedRow = 0; ///< Currently edited category row.
    const Value& m_tr;     ///< Locale subtree for translations.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
