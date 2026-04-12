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
#include "lib/ui/UIManager.hpp"
using namespace fbide;

auto ThemePage::isSyntaxStyle(const Category entry) -> bool {
    return static_cast<int>(entry) < syntaxStyleCount;
}

auto ThemePage::toItemKind(const Category entry) -> Theme::ItemKind {
    return static_cast<Theme::ItemKind>(static_cast<int>(entry) + 1);
}

ThemePage::ThemePage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_activeTheme(getConfig().getTheme())
, m_theme(getContext().getTheme()) {}

// ---------------------------------------------------------------------------
// Apply theme settings
// ---------------------------------------------------------------------------

void ThemePage::apply() {
    saveCategory();
    getContext().getTheme() = m_theme;
    getConfig().setTheme(m_activeTheme);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void ThemePage::layout() {
    createTopRow();
    makeSeparator(getVBox(), wxHORIZONTAL);

    const auto hbox = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    getVBox()->Add(hbox, 1, wxEXPAND | wxALL, 5);

    createTypeList();
    hbox->Add(m_typeList, 0, wxEXPAND | wxRIGHT, 5);

    const auto rightHBox = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    hbox->Add(rightHBox, 1, wxEXPAND);

    createColorControls(rightHBox);
    makeSeparator(rightHBox, wxVERTICAL);
    createFontControls(rightHBox);

    loadCategory();
}

void ThemePage::createTopRow() {
    const auto& lang = getContext().getLang();
    const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    getVBox()->Add(row, 0, wxEXPAND | wxALL, 5);

    makeText(row, LangId::ThemeName, wxALIGN_CENTER_VERTICAL);
    row->AddSpacer(5);

    auto themes = getConfig().getAllThemes();
    themes.insert(themes.begin(), lang[LangId::ThemeCreateNew]);
    m_themeChoice = makeChoice(row, m_activeTheme, themes);
    m_themeChoice->Bind(wxEVT_CHOICE, &ThemePage::onSelectTheme, this);

    row->AddSpacer(5);
    makeButton(row, LangId::ThemeSave)->Bind(wxEVT_BUTTON, &ThemePage::onSaveTheme, this);
}

void ThemePage::createTypeList() {
    const auto& lang = getContext().getLang();
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

    m_typeList = make_unowned<wxListBox>(this, wxID_ANY, wxDefaultPosition, wxSize(130, -1), typeNames);
    m_typeList->SetSelection(0);
    m_typeList->Bind(wxEVT_LISTBOX, &ThemePage::onSelectCategory, this);
}

void ThemePage::createColorControls(wxSizer* sizer) {
    const auto& lang = getContext().getLang();
    const auto grid = make_unowned<wxFlexGridSizer>(2, 5, 5);
    grid->AddGrowableCol(1, 1);
    sizer->Add(grid, 1, wxEXPAND | wxRIGHT, 5);

    grid->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeForeground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnFg = make_unowned<wxButton>(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, 25));
    grid->Add(m_btnFg.get());
    m_btnFg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnFg); });

    grid->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeBackground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnBg = make_unowned<wxButton>(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, 25));
    grid->Add(m_btnBg.get());
    m_btnBg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnBg); });

    grid->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFont]), 0, wxALIGN_CENTER_VERTICAL);
    m_fontChoice = make_unowned<wxChoice>(this, wxID_ANY);
    wxFontEnumerator fontEnum;
    auto fontList = fontEnum.GetFacenames();
    fontList.Sort();
    m_fontChoice->Append("");
    for (const auto& font : fontList) {
        m_fontChoice->Append(font);
    }
    grid->Add(m_fontChoice.get(), 0, wxEXPAND);
}

void ThemePage::createFontControls(wxSizer* sizer) {
    const auto& lang = getContext().getLang();
    const auto col = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(col, 0, wxLEFT, 5);

    col->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFontStyle]), 0, wxBOTTOM, 5);
    m_chkBold = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeBold]);
    col->Add(m_chkBold.get(), 0, wxBOTTOM, 5);
    m_chkItalic = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeItalic]);
    col->Add(m_chkItalic.get(), 0, wxBOTTOM, 5);
    m_chkUnderline = make_unowned<wxCheckBox>(this, wxID_ANY, lang[LangId::ThemeUnderline]);
    col->Add(m_chkUnderline.get(), 0, wxBOTTOM, 5);

    col->AddSpacer(5);
    col->Add(make_unowned<wxStaticText>(this, wxID_ANY, lang[LangId::ThemeFontSize]), 0, wxBOTTOM, 5);
    m_spinFontSize = make_unowned<wxSpinCtrl>(this, wxID_ANY, "", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 100, 12);
    col->Add(m_spinFontSize.get());
}

void ThemePage::onColorButton(wxButton* btn) {
    wxColourData data;
    data.SetColour(btn->GetBackgroundColour());
    if (wxColourDialog dlg(this, &data); dlg.ShowModal() == wxID_OK) {
        btn->SetBackgroundColour(dlg.GetColourData().GetColour());
        btn->Refresh();
    }
}

// ---------------------------------------------------------------------------
// Theme selection
// ---------------------------------------------------------------------------

void ThemePage::onSelectTheme(const wxCommandEvent&) {
    m_activeTheme = m_themeChoice->GetStringSelection();
    if (m_themeChoice->GetSelection() != 0) {
        m_theme.load(getConfig().resolvePath(m_activeTheme + ".fbt"));
        loadCategory();
    }
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    saveCategory();

    // Save existing theme?
    if (m_themeChoice->GetSelection() != 0) {
        m_theme.save();
        // saving currently active theme?
        if (m_activeTheme == getConfig().getTheme()) {
            getContext().getTheme() = m_theme;
            getContext().getUIManager().updateEditorSettigs();
        }
        return;
    }

    // creating a new theme
    const auto& lang = getContext().getLang();
    wxTextEntryDialog dlg(this, lang[LangId::ThemeEnterName], lang[LangId::ThemeParametersTitle]);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto name = dlg.GetValue().Trim().Trim(false).Lower();
    if (name.empty()) {
        return;
    }

    const wxString path = getConfig().getIdePath() + name + ".fbt";
    if (wxFileExists(path)) {
        // TODO: show overwrite file confirmation
    }
    m_theme.setPath(path);
    m_theme.save();

    m_themeChoice->Append(name);
    m_themeChoice->SetStringSelection(name);
}

// ---------------------------------------------------------------------------
// Theme category handling
// ---------------------------------------------------------------------------

void ThemePage::onSelectCategory(const wxCommandEvent& event) {
    saveCategory();
    m_category = static_cast<Category>(event.GetSelection());
    loadCategory();
}

void ThemePage::loadCategory() {
    bool enBg = true, enFont = true, enStyle = true, enSize = true;
    wxColour fg, bg;
    wxString fontName;
    int fontSize = 12, fontStyle = 0;

    if (isSyntaxStyle(m_category)) {
        const auto& st = m_theme.getStyle(toItemKind(m_category));
        fg = st.foreground;
        bg = st.background;
        fontName = st.fontName;
        fontSize = st.fontSize;
        fontStyle = static_cast<int>(st.fontStyle);
    } else {
        switch (m_category) {
        case Category::Caret:
            fg = m_theme.getDefault().caretColour;
            enBg = enFont = enStyle = enSize = false;
            break;
        case Category::LineNumbers:
            fg = m_theme.getLineNumber().foreground;
            bg = m_theme.getLineNumber().background;
            enFont = enStyle = enSize = false;
            break;
        case Category::Selection:
            fg = m_theme.getSelection().foreground;
            bg = m_theme.getSelection().background;
            enFont = enStyle = enSize = false;
            break;
        case Category::BraceMatch:
            fg = m_theme.getBrace().foreground;
            bg = m_theme.getBrace().background;
            fontStyle = static_cast<int>(m_theme.getBrace().fontStyle);
            enFont = enSize = false;
            break;
        case Category::BraceMismatch:
            fg = m_theme.getBadBrace().foreground;
            bg = m_theme.getBadBrace().background;
            fontStyle = static_cast<int>(m_theme.getBadBrace().fontStyle);
            enFont = enSize = false;
            break;
        case Category::Editor:
            fg = m_theme.getDefault().foreground;
            bg = m_theme.getDefault().background;
            fontSize = m_theme.getDefault().fontSize;
            enFont = enStyle = false;
            break;
        default:
            break;
        }
    }

    m_btnFg->Enable(true);
    m_btnBg->Enable(enBg);
    m_fontChoice->Enable(enFont);
    m_chkBold->Enable(enStyle);
    m_chkItalic->Enable(enStyle);
    m_chkUnderline->Enable(enStyle);
    m_spinFontSize->Enable(enSize);

    if (fg.IsOk()) {
        m_btnFg->SetBackgroundColour(fg);
        m_btnFg->Refresh();
    }

    if (bg.IsOk()) {
        m_btnBg->SetBackgroundColour(bg);
        m_btnBg->Refresh();
    }

    m_chkBold->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Bold)) != 0);
    m_chkItalic->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Italic)) != 0);
    m_chkUnderline->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Underline)) != 0);
    m_spinFontSize->SetValue(fontSize);

    if (enFont) {
        const auto idx = m_fontChoice->FindString(fontName);
        m_fontChoice->SetSelection(idx != wxNOT_FOUND ? idx : 0);
    }
}

void ThemePage::saveCategory() {
    const auto fg = m_btnFg->GetBackgroundColour();
    const auto bg = m_btnBg->GetBackgroundColour();
    int fs = 0;
    if (m_chkBold->GetValue()) {
        fs |= static_cast<int>(Theme::FontStyle::Bold);
    }
    if (m_chkItalic->GetValue()) {
        fs |= static_cast<int>(Theme::FontStyle::Italic);
    }
    if (m_chkUnderline->GetValue()) {
        fs |= static_cast<int>(Theme::FontStyle::Underline);
    }
    const auto fontSt = static_cast<Theme::FontStyle>(fs);

    if (isSyntaxStyle(m_category)) {
        auto& st = m_theme.getStyle(toItemKind(m_category));
        st.foreground = fg;
        st.background = bg;
        st.fontStyle = fontSt;
        st.fontSize = m_spinFontSize->GetValue();
        if (m_fontChoice->GetSelection() > 0) {
            st.fontName = m_fontChoice->GetStringSelection();
        }
    } else {
        switch (m_category) {
        case Category::Caret:
            m_theme.getDefault().caretColour = fg;
            break;
        case Category::LineNumbers:
            m_theme.getLineNumber() = { bg, fg };
            break;
        case Category::Selection:
            m_theme.getSelection() = { bg, fg };
            break;
        case Category::BraceMatch:
            m_theme.getBrace() = { bg, fg, fontSt };
            break;
        case Category::BraceMismatch:
            m_theme.getBadBrace() = { bg, fg, fontSt };
            break;
        case Category::Editor: {
            auto& def = m_theme.getDefault();
            def.foreground = fg;
            def.background = bg;
            def.fontSize = m_spinFontSize->GetValue();
            break;
        }
        default:
            break;
        }
    }
}
