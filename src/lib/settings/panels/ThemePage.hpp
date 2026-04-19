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
#include "ui/Panel.hpp"

namespace fbide {

/// Theme settings tab — category list on the left, style editor on the right.
class ThemePage final : public Panel {
public:
    NO_COPY_AND_MOVE(ThemePage)

    ThemePage(Context& ctx, wxWindow* parent);
    [[nodiscard]] auto isUnsavedNewTheme() const -> bool { return m_themeChoice->GetSelection() == 0; }
    void create() override;
    void apply() override;

private:
    auto tr(const wxString& path) const -> wxString;

    void createTopRow();
    void createCategoryList();
    void createLeftPanel();
    void createRightPanel();

    enum ControlId : int {
        ID_THEME_CHOICE = wxID_HIGHEST + 1,
        ID_SAVE_THEME,
        ID_CATEGORY_LIST,
        ID_CHK_INHERIT_FG,
        ID_CHK_INHERIT_BG,
        ID_BTN_FG,
        ID_BTN_BG,
    };

    void onSelectCategory(wxCommandEvent& event);
    void onSelectTheme(wxCommandEvent& event);
    void onSaveTheme(wxCommandEvent& event);
    void onInheritFgToggle(wxCommandEvent& event);
    void onInheritBgToggle(wxCommandEvent& event);
    void onFgClick(wxCommandEvent& event);
    void onBgClick(wxCommandEvent& event);

    void saveNewTheme(bool setActive);
    void onColorButton(wxButton* btn);
    void loadCategory();
    void saveCategory();
    void updateTitle();
    void applyCapability();
    void syncActiveThemeConfig();

    Unowned<wxListBox>    m_typeList;
    Unowned<wxChoice>     m_themeChoice;
    Unowned<wxCheckBox>   m_chkInheritFg;
    Unowned<wxCheckBox>   m_chkInheritBg;
    Unowned<wxButton>     m_btnFg;
    Unowned<wxButton>     m_btnBg;
    Unowned<wxChoice>     m_fontChoice;
    Unowned<wxStaticText> m_fontOptionsLabel;
    Unowned<wxCheckBox>   m_chkBold;
    Unowned<wxCheckBox>   m_chkItalic;
    Unowned<wxCheckBox>   m_chkUnderline;
    Unowned<wxSpinCtrl>   m_spinFontSize;
    Unowned<wxStaticText> m_lblFg;
    Unowned<wxStaticText> m_lblBg;
    Unowned<wxStaticText> m_lblFont;
    Unowned<wxStaticText> m_lblFontSize;

    wxStaticBoxSizer* m_themeBox = nullptr;

    wxString m_activeTheme;
    Theme    m_theme {};

    /// List-index → SettingsCategory. Default at index 0, rest sorted
    /// alphabetically by translated label.
    std::array<SettingsCategory, kSettingsCategoryCount> m_categoryOrder {};
    int m_selectedRow = 0;
    const Value& m_tr;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
