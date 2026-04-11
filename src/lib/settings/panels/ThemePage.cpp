//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "ThemePage.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
using namespace fbide;

namespace {
constexpr int border = 5;
}

ThemePage::ThemePage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {}

void ThemePage::layout() {
    const auto& lang = getContext().getLang();
    const auto& config = getContext().getConfig();

    wxArrayString typeNames;
    typeNames.Add(lang[LangId::ThemeComments]);
    typeNames.Add(lang[LangId::ThemeNumbers]);
    typeNames.Add(lang[LangId::ThemeKeywords1]);
    typeNames.Add(lang[LangId::ThemeStringClosed]);
    typeNames.Add(lang[LangId::ThemePreprocessor]);
    typeNames.Add(lang[LangId::ThemeOperator]);
    typeNames.Add(lang[LangId::ThemeIdentifier]);
    typeNames.Add(lang[LangId::ThemeDate]);
    typeNames.Add(lang[LangId::ThemeStringOpen]);
    typeNames.Add(lang[LangId::ThemeKeywords2]);
    typeNames.Add(lang[LangId::ThemeKeywords3]);
    typeNames.Add(lang[LangId::ThemeKeywords4]);
    typeNames.Add(lang[LangId::ThemeCaret]);
    typeNames.Add(lang[LangId::ThemeLineNumbers]);
    typeNames.Add(lang[LangId::ThemeTextSelect]);
    typeNames.Add(lang[LangId::ThemeBraceMatch]);
    typeNames.Add(lang[LangId::ThemeBraceMismatch]);
    typeNames.Add(lang[LangId::ThemeEditor]);

    m_themeTypeList = make_unowned<wxListBox>(this, wxID_ANY, wxDefaultPosition, wxSize(150, -1), typeNames);
    m_themeTypeList->SetSelection(0);

    const auto themeSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    themeSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeName]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);

    m_themeChoice = make_unowned<wxChoice>(this, wxID_ANY);
    m_themeChoice->Append(lang[LangId::ThemeCreateNew]);

    if (wxDir themeDir(config.getIdePath()); themeDir.IsOpened()) {
        wxString filename;
        if (themeDir.GetFirst(&filename, "*.fbt", wxDIR_FILES)) {
            do {
                m_themeChoice->Append(wxFileName(filename).GetName());
            } while (themeDir.GetNext(&filename));
        }
    }

    auto currentTheme = wxFileName(config.getThemeFile()).GetName();
    auto themeIdx = m_themeChoice->FindString(currentTheme);
    m_themeChoice->SetSelection(themeIdx != wxNOT_FOUND ? themeIdx : 0);

    themeSizer->Add(m_themeChoice.get(), 1, wxRIGHT, border);
    const auto btnSave = make_unowned<wxButton>(this, wxID_ANY, lang[LangId::ThemeSave]);
    themeSizer->Add(btnSave);

    const auto colorSizer = make_unowned<wxFlexGridSizer>(2, border, border);
    colorSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeForeground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnForeground = make_unowned<wxButton>(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, 25));
    colorSizer->Add(m_btnForeground.get());

    colorSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeBackground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnBackground = make_unowned<wxButton>(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, 25));
    colorSizer->Add(m_btnBackground.get());

    colorSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFont]), 0, wxALIGN_CENTER_VERTICAL);
    m_chFont = make_unowned<wxChoice>(this, wxID_ANY);
    wxFontEnumerator fontEnum;
    auto fontList = fontEnum.GetFacenames();
    fontList.Sort();
    m_chFont->Append("");
    for (const auto& font : fontList) {
        m_chFont->Append(font);
    }
    colorSizer->Add(m_chFont.get(), 0, wxEXPAND);

    colorSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFontStyle]), 0, wxALIGN_CENTER_VERTICAL);
    const auto styleSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    m_chkBold = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeBold]);
    m_chkItalic = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeItalic]);
    m_chkUnderline = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeUnderline]);
    styleSizer->Add(m_chkBold.get(), 0, wxRIGHT, border);
    styleSizer->Add(m_chkItalic.get(), 0, wxRIGHT, border);
    styleSizer->Add(m_chkUnderline.get());
    colorSizer->Add(styleSizer);

    colorSizer->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFontSize]), 0, wxALIGN_CENTER_VERTICAL);
    m_spinFontSize = make_unowned<wxSpinCtrl>(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 12);
    colorSizer->Add(m_spinFontSize.get());

    const auto rightSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    rightSizer->Add(themeSizer, 0, wxEXPAND | wxBOTTOM, border);
    rightSizer->Add(colorSizer, 0, wxEXPAND);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    mainSizer->Add(m_themeTypeList.get(), 0, wxEXPAND | wxALL, border);
    mainSizer->Add(rightSizer, 1, wxEXPAND | wxALL, border);

    SetSizer(mainSizer);
    setTypeSelection(0);

    m_themeTypeList->Bind(wxEVT_LISTBOX, &ThemePage::onThemeTypeSelected, this);
    m_themeChoice->Bind(wxEVT_CHOICE, &ThemePage::onThemeChanged, this);
    btnSave->Bind(wxEVT_BUTTON, &ThemePage::onSaveTheme, this);
    m_btnForeground->Bind(wxEVT_BUTTON, &ThemePage::onForegroundColor, this);
    m_btnBackground->Bind(wxEVT_BUTTON, &ThemePage::onBackgroundColor, this);
}

void ThemePage::apply() {
    storeTypeSelection(m_themeTypeList->GetSelection());
    if (m_themeChoice->GetSelection() > 0) {
        getContext().getConfig().setThemeFile(m_themeChoice->GetStringSelection() + ".fbt");
    }
}

void ThemePage::setTypeSelection(const int sel) {
    const auto& theme = getContext().getTheme();
    bool enableFg = true, enableBg = true, enableFont = true, enableStyle = true, enableSize = true;
    wxColour fgColour, bgColour;
    wxString fontName;
    int fontSize = 12, fontStyle = 0;

    if (sel < 12) {
        const auto& style = theme.getStyle(static_cast<Theme::ItemKind>(sel + 1));
        fgColour = style.foreground;
        bgColour = style.background;
        fontName = style.fontName;
        fontSize = style.fontSize;
        fontStyle = static_cast<int>(style.fontStyle);
    } else if (sel == 12) {
        fgColour = theme.getDefault().caretColour;
        enableBg = enableFont = enableStyle = enableSize = false;
    } else if (sel == 13) {
        fgColour = theme.getLineNumber().foreground;
        bgColour = theme.getLineNumber().background;
        enableFont = enableStyle = enableSize = false;
    } else if (sel == 14) {
        fgColour = theme.getSelection().foreground;
        bgColour = theme.getSelection().background;
        enableFont = enableStyle = enableSize = false;
    } else if (sel == 15) {
        fgColour = theme.getBrace().foreground;
        bgColour = theme.getBrace().background;
        fontStyle = static_cast<int>(theme.getBrace().fontStyle);
        enableFont = enableSize = false;
    } else if (sel == 16) {
        fgColour = theme.getBadBrace().foreground;
        bgColour = theme.getBadBrace().background;
        fontStyle = static_cast<int>(theme.getBadBrace().fontStyle);
        enableFont = enableSize = false;
    } else if (sel == 17) {
        fgColour = theme.getDefault().foreground;
        bgColour = theme.getDefault().background;
        fontSize = theme.getDefault().fontSize;
        enableFont = enableStyle = false;
    }

    m_btnForeground->Enable(enableFg);
    m_btnBackground->Enable(enableBg);
    m_chFont->Enable(enableFont);
    m_chkBold->Enable(enableStyle);
    m_chkItalic->Enable(enableStyle);
    m_chkUnderline->Enable(enableStyle);
    m_spinFontSize->Enable(enableSize);

    if (fgColour.IsOk()) { m_btnForeground->SetBackgroundColour(fgColour); }
    if (bgColour.IsOk()) { m_btnBackground->SetBackgroundColour(bgColour); }

    m_chkBold->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Bold)) != 0);
    m_chkItalic->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Italic)) != 0);
    m_chkUnderline->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Underline)) != 0);
    m_spinFontSize->SetValue(fontSize);

    if (enableFont) {
        const auto fontIdx = m_chFont->FindString(fontName);
        m_chFont->SetSelection(fontIdx != wxNOT_FOUND ? fontIdx : 0);
    }
}

void ThemePage::storeTypeSelection(const int sel) {
    auto& theme = getContext().getTheme();
    const auto fg = m_btnForeground->GetBackgroundColour();
    const auto bg = m_btnBackground->GetBackgroundColour();
    int fs = 0;
    if (m_chkBold->GetValue()) { fs |= static_cast<int>(Theme::FontStyle::Bold); }
    if (m_chkItalic->GetValue()) { fs |= static_cast<int>(Theme::FontStyle::Italic); }
    if (m_chkUnderline->GetValue()) { fs |= static_cast<int>(Theme::FontStyle::Underline); }

    if (sel < 12) {
        auto& style = theme.getStyle(static_cast<Theme::ItemKind>(sel + 1));
        style.foreground = fg;
        style.background = bg;
        style.fontStyle = static_cast<Theme::FontStyle>(fs);
        style.fontSize = m_spinFontSize->GetValue();
        if (m_chFont->GetSelection() > 0) { style.fontName = m_chFont->GetStringSelection(); }
    } else if (sel == 12) { theme.getDefault().caretColour = fg; }
    else if (sel == 13) { theme.getLineNumber() = { bg, fg }; }
    else if (sel == 14) { theme.getSelection() = { bg, fg }; }
    else if (sel == 15) { theme.getBrace() = { bg, fg, static_cast<Theme::FontStyle>(fs) }; }
    else if (sel == 16) { theme.getBadBrace() = { bg, fg, static_cast<Theme::FontStyle>(fs) }; }
    else if (sel == 17) { theme.getDefault().foreground = fg; theme.getDefault().background = bg; theme.getDefault().fontSize = m_spinFontSize->GetValue(); }
}

void ThemePage::onThemeTypeSelected(wxCommandEvent& event) {
    storeTypeSelection(m_themeTypeOld);
    m_themeTypeOld = event.GetSelection();
    setTypeSelection(m_themeTypeOld);
}

void ThemePage::onThemeChanged(wxCommandEvent&) {
    if (m_themeChoice->GetSelection() <= 0) { return; }
    getContext().getTheme().load(getContext().getConfig().getIdePath() + m_themeChoice->GetStringSelection() + ".fbt");
    setTypeSelection(m_themeTypeList->GetSelection());
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    storeTypeSelection(m_themeTypeList->GetSelection());
    if (m_themeChoice->GetSelection() == 0) {
        const auto& lang = getContext().getLang();
        wxTextEntryDialog dlg(this, lang[LangId::ThemeEnterName], lang[LangId::ThemeParametersTitle]);
        if (dlg.ShowModal() != wxID_OK) { return; }
        auto name = dlg.GetValue().Trim().Trim(false).Lower();
        if (name.empty()) { return; }
        getContext().getTheme().load(getContext().getConfig().getIdePath() + name + ".fbt");
        m_themeChoice->Append(name);
        m_themeChoice->SetStringSelection(name);
    }
    getContext().getTheme().save();
}

void ThemePage::onForegroundColor(wxCommandEvent&) {
    wxColourData data;
    data.SetColour(m_btnForeground->GetBackgroundColour());
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        m_btnForeground->SetBackgroundColour(dlg.GetColourData().GetColour());
        m_btnForeground->Refresh();
    }
}

void ThemePage::onBackgroundColor(wxCommandEvent&) {
    wxColourData data;
    data.SetColour(m_btnBackground->GetBackgroundColour());
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        m_btnBackground->SetBackgroundColour(dlg.GetColourData().GetColour());
        m_btnBackground->Refresh();
    }
}
