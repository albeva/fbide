//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "ThemePage.hpp"
#include "app/Context.hpp"
#include "config/Config.hpp"
#include "config/ThemeOld.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

auto ThemePage::isSyntaxStyle(const Category entry) -> bool {
    return static_cast<int>(entry) < syntaxStyleCount;
}

auto ThemePage::toItemKind(const Category entry) -> ThemeOld::ItemKind {
    return static_cast<ThemeOld::ItemKind>(static_cast<int>(entry) + 1);
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

void ThemePage::create() {
    createTopRow();

    hbox(m_activeTheme, { .proportion = 1, .border = 0 }, [&] {
        m_themeBox = wxDynamicCast(currentSizer(), wxStaticBoxSizer);
        createCategoryList();
        createLeftPanel();
        separator();
        createRightPanel();
    });

    updateTitle();
    loadCategory();
}

void ThemePage::createTopRow() {
    hbox(tr("dialogs.settings.themes.name"), { .center = true, .border = 0 }, [&] {
        auto themes = getConfig().getAllThemes();
        themes.insert(themes.begin(), tr("dialogs.settings.themes.createNew"));

        m_themeChoice = choice(m_activeTheme, themes, { .proportion = 1, .expand = false });
        m_themeChoice->Bind(wxEVT_CHOICE, &ThemePage::onSelectTheme, this);

        const auto save = button(tr("dialogs.settings.themes.save"), { .expand = false });
        save->Bind(wxEVT_BUTTON, &ThemePage::onSaveTheme, this);
    });
}

void ThemePage::createCategoryList() {
    wxArrayString typeNames;
    typeNames.Add(tr("dialogs.settings.themes.categories.comments"));
    typeNames.Add(tr("dialogs.settings.themes.categories.numbers"));
    typeNames.Add(tr("dialogs.settings.themes.categories.keywords1"));
    typeNames.Add(tr("dialogs.settings.themes.categories.stringClosed"));
    typeNames.Add(tr("dialogs.settings.themes.categories.preprocessor"));
    typeNames.Add(tr("dialogs.settings.themes.categories.operator"));
    typeNames.Add(tr("dialogs.settings.themes.categories.identifier"));
    typeNames.Add(tr("dialogs.settings.themes.categories.date"));
    typeNames.Add(tr("dialogs.settings.themes.categories.stringOpen"));
    typeNames.Add(tr("dialogs.settings.themes.categories.keywords2"));
    typeNames.Add(tr("dialogs.settings.themes.categories.keywords3"));
    typeNames.Add(tr("dialogs.settings.themes.categories.keywords4"));
    typeNames.Add(tr("dialogs.settings.themes.categories.caret"));
    typeNames.Add(tr("dialogs.settings.themes.categories.lineNumbers"));
    typeNames.Add(tr("dialogs.settings.themes.categories.textSelect"));
    typeNames.Add(tr("dialogs.settings.themes.categories.braceMatch"));
    typeNames.Add(tr("dialogs.settings.themes.categories.braceMismatch"));
    typeNames.Add(tr("dialogs.settings.themes.categories.editor"));

    m_typeList = make_unowned<wxListBox>(currentParent(), wxID_ANY, wxDefaultPosition, wxSize(130, 200), typeNames);
    m_typeList->SetSelection(0);
    m_typeList->Bind(wxEVT_LISTBOX, &ThemePage::onSelectCategory, this);
    add(m_typeList, {});
}

void ThemePage::createLeftPanel() {
    vbox({ .proportion = 2, .border = 0 }, [&] {
        auto lbl = text(tr("dialogs.settings.themes.foreground"), {});
        m_btnFg = button(wxString {});
        m_btnFg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnFg); });
        connect(lbl, m_btnFg);

        spacer();

        lbl = text(tr("dialogs.settings.themes.background"), {});
        m_btnBg = button(wxString {});
        m_btnBg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnBg); });
        connect(lbl, m_btnBg);

        spacer();

        lbl = text(tr("dialogs.settings.themes.font"), {});
        m_fontChoice = make_unowned<wxChoice>(currentParent(), wxID_ANY);
        auto fonts = getConfig().getAllFixedWidthFonts();
        fonts.insert(fonts.begin(), "");
        m_fontChoice->Append(fonts);
        add(m_fontChoice);
        connect(lbl, m_fontChoice);
    });
}

void ThemePage::createRightPanel() {
    vbox({ .proportion = 1, .border = 0 }, [&] {
        m_chkBold = checkBox(tr("dialogs.settings.themes.bold"));
        m_chkItalic = checkBox(tr("dialogs.settings.themes.italic"));
        m_chkUnderline = checkBox(tr("dialogs.settings.themes.underline"));

        spacer();

        m_spinFontSize = spinCtrl(tr("dialogs.settings.themes.fontSize"), 8, 64, {});
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
    updateTitle();
    if (not isUnsavedNewTheme()) {
        m_theme.load(getConfig().resolvePath(m_activeTheme + "." + Config::THEME_EXT));
        loadCategory();
    }
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    saveCategory();

    // Save existing theme?
    if (isUnsavedNewTheme()) {
        saveNewTheme(false);
        updateTitle();
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
    wxTextEntryDialog dlg(this, tr("dialogs.settings.themes.enterName"), tr("dialogs.settings.themes.nameDialogTitle"));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto name = dlg.GetValue().Trim().Trim(false).Lower();
    if (name.empty()) {
        return;
    }

    const wxFileName path = getConfig().getAppSettingsPath() + name + "." + Config::THEME_EXT;
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
    ThemeOld::FontStyle fontStyle;

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
    ThemeOld::FontStyle fontSt;
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

void ThemePage::updateTitle() {
    m_themeBox->GetStaticBox()->SetLabel(m_activeTheme.Capitalize());
}
