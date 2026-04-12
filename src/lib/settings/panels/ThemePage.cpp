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

    if (isUnsavedNewTheme()) {
        saveNewTheme(true);
    } else {
        getContext().getTheme() = m_theme;
        getConfig().setTheme(m_activeTheme);
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void ThemePage::layout() {
    createTopRow();
    separator();

    hbox({ .proportion = 1, .flag = wxEXPAND | wxALL, .border = 0 }, [&] {
        createCategoryList();
        createLeftPanel();
        separator();
        createRightPanel();
    });

    loadCategory();
}

void ThemePage::createTopRow() {
    const auto& lang = getContext().getLang();
    hbox({ .flag = wxEXPAND | wxALL }, [&] {
        text(LangId::ThemeName, { .flag = wxALIGN_CENTER_VERTICAL });
        spacer();

        auto themes = getConfig().getAllThemes();
        themes.insert(themes.begin(), lang[LangId::ThemeCreateNew]);
        m_themeChoice = choice(m_activeTheme, themes, { .proportion = 1, .flag = wxEXPAND | wxALIGN_CENTER_VERTICAL });
        m_themeChoice->Bind(wxEVT_CHOICE, &ThemePage::onSelectTheme, this);

        spacer();
        const auto save = button(LangId::ThemeSave, { .flag = wxALIGN_CENTER_VERTICAL });
        save->Bind(wxEVT_BUTTON, &ThemePage::onSaveTheme, this);
    });
}

void ThemePage::createCategoryList() {
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
    getCurrentSizer()->Add(m_typeList, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT, 5);
}

void ThemePage::createLeftPanel() {
    vbox({ .proportion = 1, .flag = wxEXPAND, .border = 0 }, [&] {
        const auto PAD = wxEXPAND | wxLEFT | wxTOP | wxRIGHT;

        text(LangId::ThemeForeground, { .flag = PAD });
        m_btnFg = button(LangId::EmptyString, { .flag = wxALL | wxEXPAND });
        m_btnFg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnFg); });

        spacer();

        text(LangId::ThemeBackground, { .flag = PAD });
        m_btnBg = button(LangId::EmptyString, { .flag = wxALL | wxEXPAND });
        m_btnBg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnBg); });

        spacer();

        text(LangId::ThemeFont, { .flag = PAD });
        m_fontChoice = make_unowned<wxChoice>(this, wxID_ANY);
        auto fonts = getConfig().getAllFixedWidthFonts();
        fonts.insert(fonts.begin(), "");
        m_fontChoice->Append(fonts);
        getCurrentSizer()->Add(m_fontChoice.get(), 0, wxEXPAND | wxALL, DEFAULT_PAD);
    });
}

void ThemePage::createRightPanel() {
    vbox({ .proportion = 0, .flag = wxEXPAND, .border = 0 }, [&] {
        const auto flags = wxEXPAND | wxLEFT | wxTOP | wxRIGHT;

        text(LangId::ThemeFontStyle, { .flag = flags });
        m_chkBold = checkBox(LangId::ThemeBold);
        m_chkItalic = checkBox(LangId::ThemeItalic);
        m_chkUnderline = checkBox(LangId::ThemeUnderline);

        spacer();

        text(LangId::ThemeFontSize, { .flag = flags });
        m_spinFontSize = spinCtrl(LangId::EmptyString, 8, 64);
    });
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
    if (not isUnsavedNewTheme()) {
        m_theme.load(getConfig().resolvePath(m_activeTheme + ".fbt"));
        loadCategory();
    }
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    saveCategory();

    // Save existing theme?
    if (isUnsavedNewTheme()) {
        saveNewTheme(false);
    }

    m_theme.save();

    // saving currently active theme?
    if (m_activeTheme == getConfig().getTheme()) {
        getContext().getTheme() = m_theme;
        getContext().getUIManager().updateEditorSettigs();
    }
}

void ThemePage::saveNewTheme(const bool setActive) {
    if (not isUnsavedNewTheme()) {
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

    const wxFileName path = getConfig().getIdePath() + name + ".fbt";
    if (not path.IsOk() or path.Exists()) {
        // TODO: show warning?
        wxLogWarning("Unable to save theme as %s", path.GetAbsolutePath());
        return;
    }

    m_theme.setPath(path.GetAbsolutePath());
    m_theme.save();

    m_themeChoice->Append(name);
    m_themeChoice->SetStringSelection(name);

    if (setActive) {
        getConfig().setTheme(name);
        getContext().getTheme() = m_theme;
    }
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
    int fontSize = 12;
    Theme::FontStyle fontStyle;

    if (isSyntaxStyle(m_category)) {
        const auto& st = m_theme.getStyle(toItemKind(m_category));
        fg = st.foreground;
        bg = st.background;
        fontName = st.fontName;
        fontSize = st.fontSize;
        fontStyle = st.fontStyle;
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
            fontStyle = m_theme.getBrace().fontStyle;
            enFont = enSize = false;
            break;
        case Category::BraceMismatch:
            fg = m_theme.getBadBrace().foreground;
            bg = m_theme.getBadBrace().background;
            fontStyle = m_theme.getBadBrace().fontStyle;
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

    m_chkBold->SetValue(fontStyle.bold);
    m_chkItalic->SetValue(fontStyle.italic);
    m_chkUnderline->SetValue(fontStyle.underline);
    m_spinFontSize->SetValue(fontSize);

    if (enFont) {
        const auto idx = m_fontChoice->FindString(fontName);
        m_fontChoice->SetSelection(idx != wxNOT_FOUND ? idx : 0);
    }
}

void ThemePage::saveCategory() {
    const auto fg = m_btnFg->GetBackgroundColour();
    const auto bg = m_btnBg->GetBackgroundColour();
    Theme::FontStyle fontSt;
    fontSt.bold = m_chkBold->GetValue();
    fontSt.italic = m_chkItalic->GetValue();
    fontSt.underline = m_chkUnderline->GetValue();

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
