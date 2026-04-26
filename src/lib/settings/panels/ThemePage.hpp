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

    ThemePage(Context& ctx, wxWindow* parent);
    [[nodiscard]] auto isUnsavedNewTheme() const -> bool { return m_themeChoice->GetSelection() == 0; }
    void create() override;
    void apply() override;

    /// Enumerate all fixed-width system fonts — used only by this panel.
    [[nodiscard]] static auto getAllFixedWidthFonts() -> std::vector<wxString>;

private:
    auto tr(const wxString& path, const wxString& def = wxEmptyString) const -> wxString;

    void createTopRow();
    void createCategoryList();
    void createLeftPanel();
    void createRightPanel();

    enum ControlId : int {
        ID_THEME_CHOICE = wxID_HIGHEST + 1,
        ID_SAVE_THEME,
        ID_CATEGORY_LIST
    };

    void onSelectCategory(wxCommandEvent& event);
    void onSelectTheme(wxCommandEvent& event);
    void onSaveTheme(wxCommandEvent& event);

    void saveNewTheme(bool setActive);
    void loadCategory();
    void saveCategory();
    void updateTitle();
    void applyCapability();
    void syncActiveThemeConfig();

    Unowned<wxListBox> m_typeList;
    Unowned<wxChoice> m_themeChoice;
    Unowned<ColorPicker> m_fgPicker;
    Unowned<ColorPicker> m_bgPicker;
    Unowned<ColorPicker> m_separatorPicker;
    Unowned<wxChoice> m_fontChoice;
    Unowned<wxStaticText> m_fontOptionsLabel;
    Unowned<wxCheckBox> m_chkBold;
    Unowned<wxCheckBox> m_chkItalic;
    Unowned<wxCheckBox> m_chkUnderline;
    Unowned<wxSpinCtrl> m_spinFontSize;
    Unowned<wxStaticText> m_lblFont;
    Unowned<wxStaticText> m_lblFontSize;

    wxStaticBoxSizer* m_themeBox = nullptr;

    static constexpr int FILE_OFFSET = 1;
    Theme m_theme {};
    wxString m_activeTheme;
    std::vector<wxString> m_themeFiles;

    int m_selectedRow = 0;
    const Value& m_tr;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
