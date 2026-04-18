//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/config/Theme.hpp"
#include "lib/ui/Panel.hpp"

namespace fbide {

/// Theme settings tab — style type selection, colors, fonts.
class ThemePage final : public Panel {
public:
    NO_COPY_AND_MOVE(ThemePage)

    ThemePage(Context& ctx, wxWindow* parent);
    [[nodiscard]] auto isUnsavedNewTheme() const -> bool { return m_themeChoice->GetSelection() == 0; }
    void create() override;
    void apply() override;

private:
    /// All entries in the theme type listbox.
    /// First 12 are syntax styles (map to Theme::ItemKind + 1),
    /// rest are special theme elements.
    enum class Category : int {
        Comments = 0,
        Numbers,
        Keywords1,
        StringClosed,
        Preprocessor,
        Operator,
        Identifier,
        Date,
        StringOpen,
        Keywords2,
        Keywords3,
        Keywords4,
        // Constant,
        // Asm,
        // -- special entries below --
        Caret,
        LineNumbers,
        Selection,
        BraceMatch,
        BraceMismatch,
        Editor,
    };

    static constexpr int syntaxStyleCount = 12;
    static constexpr int typeEntryCount = 20;

    /// Check if entry is a syntax style (vs special element).
    [[nodiscard]] static auto isSyntaxStyle(Category entry) -> bool;

    /// Convert syntax style entry to Theme::ItemKind.
    [[nodiscard]] static auto toItemKind(Category entry) -> Theme::ItemKind;

    void createTopRow();
    void createCategoryList();
    void createLeftPanel();
    void createRightPanel();

    void onSelectCategory(const wxCommandEvent& event);
    void onSelectTheme(const wxCommandEvent& event);
    void saveNewTheme(bool setActive);
    void onSaveTheme(wxCommandEvent& event);
    void onColorButton(wxButton* btn);
    void loadCategory();
    void saveCategory();
    void updateTitle();

    Unowned<wxListBox> m_typeList;
    Unowned<wxChoice> m_themeChoice;
    Unowned<wxButton> m_btnFg;
    Unowned<wxButton> m_btnBg;
    Unowned<wxChoice> m_fontChoice;
    Unowned<wxCheckBox> m_chkBold;
    Unowned<wxCheckBox> m_chkItalic;
    Unowned<wxCheckBox> m_chkUnderline;
    Unowned<wxSpinCtrl> m_spinFontSize;

    wxStaticBoxSizer* m_themeBox = nullptr;

    wxString m_activeTheme;
    Category m_category = Category::Comments;
    Theme m_theme;
};

} // namespace fbide
