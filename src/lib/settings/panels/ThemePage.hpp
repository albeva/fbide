//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Panel.hpp"

namespace fbide {

/// Theme settings tab — style type selection, colors, fonts.
class ThemePage final : public Panel {
public:
    explicit ThemePage(Context& ctx, wxWindow* parent);
    void layout() override;
    void apply() override;

private:
    void onThemeTypeSelected(wxCommandEvent& event);
    void onThemeChanged(wxCommandEvent& event);
    void onSaveTheme(wxCommandEvent& event);
    void onForegroundColor(wxCommandEvent& event);
    void onBackgroundColor(wxCommandEvent& event);
    void setTypeSelection(int sel);
    void storeTypeSelection(int sel);

    Unowned<wxListBox> m_themeTypeList;
    Unowned<wxChoice> m_themeChoice;
    Unowned<wxButton> m_btnForeground;
    Unowned<wxButton> m_btnBackground;
    Unowned<wxChoice> m_chFont;
    Unowned<wxCheckBox> m_chkBold;
    Unowned<wxCheckBox> m_chkItalic;
    Unowned<wxCheckBox> m_chkUnderline;
    Unowned<wxSpinCtrl> m_spinFontSize;
    int m_themeTypeOld = 0;
};

} // namespace fbide
