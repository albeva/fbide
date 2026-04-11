//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "SettingsDialog.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr int border = 5;

enum ControlId : int {
    ThemeTypeList = wxID_HIGHEST + 100,
    ThemeChoice,
    SaveTheme,
    ForegroundColor,
    BackgroundColor,
    FontChoice,
    KeywordGroup,
    // CompilerBrowse,
    // HelpBrowse,
};
} // namespace

SettingsDialog::SettingsDialog(wxWindow* parent, Context& ctx)
: wxDialog(parent, wxID_ANY, ctx.getLang()[LangId::SettingsTitle],
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
, m_ctx(ctx) {}

void SettingsDialog::create() {
    const auto notebook = make_unowned<wxNotebook>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

    notebook->AddPage(createGeneralTab(notebook), m_ctx.getLang()[LangId::SettingsGeneral]);
    notebook->AddPage(createThemesTab(notebook), m_ctx.getLang()[LangId::SettingsThemes]);
    notebook->AddPage(createKeywordsTab(notebook), m_ctx.getLang()[LangId::SettingsKeywords]);
    notebook->AddPage(createCompilerTab(notebook), m_ctx.getLang()[LangId::SettingsCompiler]);

    auto* btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

    const auto mainSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, border);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);
    SetSizer(mainSizer);
    SetMinSize(wxSize(500, 400));
    Fit();
    Centre();

    Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&) {
            applyChanges();
            EndModal(wxID_OK);
        },
        wxID_OK
    );
}

// -- General Tab --

auto SettingsDialog::createGeneralTab(wxNotebook* notebook) -> Unowned<wxPanel> {
    const auto& lang = m_ctx.getLang();
    const auto& config = m_ctx.getConfig();
    auto panel = make_unowned<wxPanel>(notebook);

    // Helper to create checkbox and set value
    auto chk = [&](Unowned<wxCheckBox>& ctrl, const LangId id, const bool value) {
        ctrl = make_unowned<wxCheckBox>(panel, wxID_ANY, lang[id]);
        ctrl->SetValue(value);
    };

    // Editor settings — paired rows
    const auto editorBox = make_unowned<wxStaticBoxSizer>(wxVERTICAL, panel, lang[LangId::SettingsEditorSettings]);

    // Helper: add a row with two checkboxes side by side
    auto row = [&](Unowned<wxCheckBox>& left, LangId leftId, bool leftVal,
                    Unowned<wxCheckBox>& right, LangId rightId, bool rightVal) {
        chk(left, leftId, leftVal);
        chk(right, rightId, rightVal);
        const auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
        sizer->Add(left.get(), 1);
        sizer->Add(right.get(), 1);
        editorBox->Add(sizer, 0, wxEXPAND | wxALL, border);
    };

    row(m_chkAutoIndent, LangId::SettingsAutoIndent, config.getAutoIndent(),
        m_chkSyntaxHighlight, LangId::SettingsSyntaxHighlight, config.getSyntaxHighlight());
    row(m_chkIndentGuides, LangId::SettingsIndentGuides, config.getIndentGuide(),
        m_chkLineNumbers, LangId::SettingsLineNumbers, config.getLineNumbers());
    row(m_chkWhiteSpace, LangId::SettingsWhitespace, config.getWhiteSpace(),
        m_chkRightMargin, LangId::SettingsRightMargin, config.getLongLine());
    row(m_chkLineEndings, LangId::SettingsLineEndings, config.getDisplayEOL(),
        m_chkFoldMargin, LangId::SettingsFoldMargin, config.getFolderMargin());
    row(m_chkBraceHighlight, LangId::SettingsBraceHighlight, config.getBraceHighlight(),
        m_chkSplashScreen, LangId::SettingsSplashScreen, config.getSplashScreen());

    // Separator
    editorBox->Add(make_unowned<wxStaticLine>(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, border);

    // Spin controls — [spin label] [spin label] in one row
    const auto spinRow = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    m_spinEdgeColumn = make_unowned<wxSpinCtrl>(panel, wxID_ANY, "", wxDefaultPosition, wxSize(50, -1), wxSP_ARROW_KEYS, 1, 200, config.getEdgeColumn());
    spinRow->Add(m_spinEdgeColumn.get(), 0, wxRIGHT, border);
    spinRow->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::SettingsRightMarginWidth]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border * 4);

    m_spinTabSize = make_unowned<wxSpinCtrl>(panel, wxID_ANY, "", wxDefaultPosition, wxSize(50, -1), wxSP_ARROW_KEYS, 1, 16, config.getTabSize());
    spinRow->Add(m_spinTabSize.get(), 0, wxRIGHT, border);
    spinRow->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::SettingsTabSize]), 0, wxALIGN_CENTER_VERTICAL);

    editorBox->Add(spinRow, 0, wxALL, border);

    // Language section
    const auto langBox = make_unowned<wxStaticBoxSizer>(wxVERTICAL, panel, lang[LangId::SettingsLanguage]);

    const auto langSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    langSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::SettingsLanguageSelect]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
    m_chLanguage = make_unowned<wxChoice>(panel, wxID_ANY);

    const wxString langDir = config.getIdePath() + "lang/";
    if (const wxDir dir(langDir); dir.IsOpened()) {
        wxString filename;
        if (dir.GetFirst(&filename, "*.fbl", wxDIR_FILES)) {
            do {
                auto name = wxFileName(filename).GetName();
                m_chLanguage->Append(name);
                if (name == config.getLanguage()) {
                    m_chLanguage->SetSelection(static_cast<int>(m_chLanguage->GetCount()) - 1);
                }
            } while (dir.GetNext(&filename));
        }
    }
    langSizer->Add(m_chLanguage.get(), 1);
    langBox->Add(langSizer, 0, wxEXPAND | wxALL, border);
    langBox->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::SettingsLanguageRestart]), 0, wxALL, border);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(editorBox, 0, wxEXPAND | wxALL, border);
    sizer->Add(langBox, 0, wxEXPAND | wxALL, border);
    panel->SetSizer(sizer);
    return panel;
}

// -- Themes Tab --

auto SettingsDialog::createThemesTab(wxNotebook* notebook) -> Unowned<wxPanel> {
    const auto& lang = m_ctx.getLang();
    const auto& config = m_ctx.getConfig();
    auto panel = make_unowned<wxPanel>(notebook);

    // Style type list (left side)
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

    m_themeTypeList = make_unowned<wxListBox>(panel, ControlId::ThemeTypeList, wxDefaultPosition, wxSize(150, -1), typeNames);
    m_themeTypeList->SetSelection(0);

    // Theme choice + save
    const auto themeSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    themeSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeName]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);

    m_themeChoice = make_unowned<wxChoice>(panel, ControlId::ThemeChoice);
    m_themeChoice->Append(lang[LangId::ThemeCreateNew]);

    // Scan for theme files
    wxString ideDir = config.getIdePath();
    if (wxDir themeDir(ideDir); themeDir.IsOpened()) {
        wxString filename;
        if (themeDir.GetFirst(&filename, "*.fbt", wxDIR_FILES)) {
            do {
                auto name = wxFileName(filename).GetName();
                m_themeChoice->Append(name);
            } while (themeDir.GetNext(&filename));
        }
    }

    // Select current theme
    auto currentTheme = wxFileName(config.getThemeFile()).GetName();
    auto themeIdx = m_themeChoice->FindString(currentTheme);
    m_themeChoice->SetSelection(themeIdx != wxNOT_FOUND ? themeIdx : 0);

    themeSizer->Add(m_themeChoice.get(), 1, wxRIGHT, border);
    const auto btnSave = make_unowned<wxButton>(panel, ControlId::SaveTheme, lang[LangId::ThemeSave]);
    themeSizer->Add(btnSave);

    // Color buttons
    const auto colorSizer = make_unowned<wxFlexGridSizer>(2, border, border);
    colorSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeForeground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnForeground = make_unowned<wxButton>(panel, ControlId::ForegroundColor, "", wxDefaultPosition, wxSize(80, 25));
    colorSizer->Add(m_btnForeground.get());

    colorSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeBackground]), 0, wxALIGN_CENTER_VERTICAL);
    m_btnBackground = make_unowned<wxButton>(panel, ControlId::BackgroundColor, "", wxDefaultPosition, wxSize(80, 25));
    colorSizer->Add(m_btnBackground.get());

    // Font
    colorSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeFont]), 0, wxALIGN_CENTER_VERTICAL);
    m_chFont = make_unowned<wxChoice>(panel, ControlId::FontChoice);
    wxFontEnumerator fontEnum;
    auto fontList = fontEnum.GetFacenames();
    fontList.Sort();
    m_chFont->Append("");
    for (const auto& font : fontList) {
        m_chFont->Append(font);
    }
    colorSizer->Add(m_chFont.get(), 0, wxEXPAND);

    // Font style
    colorSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeFontStyle]), 0, wxALIGN_CENTER_VERTICAL);
    const auto styleSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    m_chkBold = make_unowned<wxCheckBox>(panel, wxID_ANY, lang[LangId::ThemeBold]);
    m_chkItalic = make_unowned<wxCheckBox>(panel, wxID_ANY, lang[LangId::ThemeItalic]);
    m_chkUnderline = make_unowned<wxCheckBox>(panel, wxID_ANY, lang[LangId::ThemeUnderline]);
    styleSizer->Add(m_chkBold.get(), 0, wxRIGHT, border);
    styleSizer->Add(m_chkItalic.get(), 0, wxRIGHT, border);
    styleSizer->Add(m_chkUnderline.get());
    colorSizer->Add(styleSizer);

    // Font size
    colorSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeFontSize]), 0, wxALIGN_CENTER_VERTICAL);
    m_spinFontSize = make_unowned<wxSpinCtrl>(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, 12);
    colorSizer->Add(m_spinFontSize.get());

    // Right side layout
    const auto rightSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    rightSizer->Add(themeSizer, 0, wxEXPAND | wxBOTTOM, border);
    rightSizer->Add(colorSizer, 0, wxEXPAND);

    // Main layout
    const auto mainSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    mainSizer->Add(m_themeTypeList.get(), 0, wxEXPAND | wxALL, border);
    mainSizer->Add(rightSizer, 1, wxEXPAND | wxALL, border);

    panel->SetSizer(mainSizer);

    // Load initial theme data
    setTypeSelection(0);

    // Bind events
    m_themeTypeList->Bind(wxEVT_LISTBOX, &SettingsDialog::onThemeTypeSelected, this);
    m_themeChoice->Bind(wxEVT_CHOICE, &SettingsDialog::onThemeChanged, this);
    btnSave->Bind(wxEVT_BUTTON, &SettingsDialog::onSaveTheme, this);
    m_btnForeground->Bind(wxEVT_BUTTON, &SettingsDialog::onForegroundColor, this);
    m_btnBackground->Bind(wxEVT_BUTTON, &SettingsDialog::onBackgroundColor, this);

    return panel;
}

// -- Keywords Tab --

auto SettingsDialog::createKeywordsTab(wxNotebook* notebook) -> Unowned<wxPanel> {
    const auto& lang = m_ctx.getLang();
    const auto& keywords = m_ctx.getKeywords();
    auto panel = make_unowned<wxPanel>(notebook);

    // Copy keyword groups
    for (int idx = 0; idx < 4; idx++) {
        m_keywordGroups[static_cast<size_t>(idx)] = keywords.getGroup(idx);
    }

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    const auto groupSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    groupSizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::ThemeSelectGroup]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
    m_chKeywordGroup = make_unowned<wxChoice>(panel, ControlId::KeywordGroup);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup1]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup2]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup3]);
    m_chKeywordGroup->Append(lang[LangId::ThemeGroup4]);
    m_chKeywordGroup->SetSelection(0);
    groupSizer->Add(m_chKeywordGroup.get(), 1);
    sizer->Add(groupSizer, 0, wxEXPAND | wxALL, border);

    m_textKeywords = make_unowned<wxTextCtrl>(panel, wxID_ANY, m_keywordGroups[0],
        wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_WORDWRAP);
    sizer->Add(m_textKeywords.get(), 1, wxEXPAND | wxALL, border);

    panel->SetSizer(sizer);

    m_chKeywordGroup->Bind(wxEVT_CHOICE, &SettingsDialog::onKeywordGroupChanged, this);
    return panel;
}

// -- Compiler Tab --

auto SettingsDialog::createCompilerTab(wxNotebook* notebook) -> Unowned<wxPanel> {
    const auto& lang = m_ctx.getLang();
    const auto& config = m_ctx.getConfig();
    auto panel = make_unowned<wxPanel>(notebook);

    // Helper: create a labeled static box with a text field and optional browse button
    auto makeFieldBox = [&](const wxString& label, const wxString& value, Unowned<wxTextCtrl>& ctrl, wxButton** browseBtn = nullptr) {
        const auto box = make_unowned<wxStaticBoxSizer>(wxVERTICAL, panel, label);
        const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
        ctrl = make_unowned<wxTextCtrl>(panel, wxID_ANY, value);
        row->Add(ctrl.get(), 1, wxEXPAND);
        if (browseBtn != nullptr) {
            *browseBtn = make_unowned<wxButton>(panel, wxID_ANY, "...", wxDefaultPosition, wxSize(30, -1));
            row->Add(*browseBtn, 0, wxLEFT, border);
        }
        box->Add(row, 0, wxEXPAND | wxALL, border);
        return box;
    };

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(make_unowned<wxStaticText>(panel, wxID_ANY, lang[LangId::SettingsCompilerAndPaths]), 0, wxALL, border);

    wxButton* btnCompiler = nullptr;
    sizer->Add(makeFieldBox(lang[LangId::SettingsCompilerPath], config.getCompilerPath(), m_textCompilerPath, &btnCompiler), 0, wxEXPAND | wxALL, border);
    sizer->Add(makeFieldBox(lang[LangId::SettingsCompilerCommand], config.getCompileCommand(), m_textCompileCommand), 0, wxEXPAND | wxALL, border);
    sizer->Add(makeFieldBox(lang[LangId::SettingsRunCmd], config.getRunCommand(), m_textRunCommand), 0, wxEXPAND | wxALL, border);

    wxButton* btnHelp = nullptr;
    sizer->Add(makeFieldBox(lang[LangId::SettingsHelpFile], config.getHelpFile(), m_textHelpFile, &btnHelp), 0, wxEXPAND | wxALL, border);

    panel->SetSizer(sizer);

    btnCompiler->Bind(wxEVT_BUTTON, &SettingsDialog::onCompilerBrowse, this);
    btnHelp->Bind(wxEVT_BUTTON, &SettingsDialog::onHelpBrowse, this);
    return panel;
}

// -- Theme Type Selection --

void SettingsDialog::setTypeSelection(const int sel) {
    const auto& theme = m_ctx.getTheme();
    bool enableFg = true;
    bool enableBg = true;
    bool enableFont = true;
    bool enableStyle = true;
    bool enableSize = true;
    wxColour fgColour;
    wxColour bgColour;
    wxString fontName;
    int fontSize = 12;
    int fontStyle = 0;

    // TODO: reuse identifiers from Theme, they should match
    // Indices 0-11 = syntax styles (Comment..Keyword4), 12=Caret, 13=LineNum, 14=Select, 15=Brace, 16=BadBrace, 17=Editor
    if (sel < 12) {
        // Syntax style (theme ItemKind 1-14, but list starts at Comment=1)
        const auto& style = theme.getStyle(static_cast<Theme::ItemKind>(sel + 1));
        fgColour = style.foreground;
        bgColour = style.background;
        fontName = style.fontName;
        fontSize = style.fontSize;
        fontStyle = static_cast<int>(style.fontStyle);
    } else if (sel == 12) { // Caret
        fgColour = theme.getDefault().caretColour;
        enableBg = false;
        enableFont = false;
        enableStyle = false;
        enableSize = false;
    } else if (sel == 13) { // Line numbers
        fgColour = theme.getLineNumber().foreground;
        bgColour = theme.getLineNumber().background;
        enableFont = false;
        enableStyle = false;
        enableSize = false;
    } else if (sel == 14) { // Selection
        fgColour = theme.getSelection().foreground;
        bgColour = theme.getSelection().background;
        enableFont = false;
        enableStyle = false;
        enableSize = false;
    } else if (sel == 15) { // Brace match
        fgColour = theme.getBrace().foreground;
        bgColour = theme.getBrace().background;
        fontStyle = static_cast<int>(theme.getBrace().fontStyle);
        enableFont = false;
        enableSize = false;
    } else if (sel == 16) { // Brace mismatch
        fgColour = theme.getBadBrace().foreground;
        bgColour = theme.getBadBrace().background;
        fontStyle = static_cast<int>(theme.getBadBrace().fontStyle);
        enableFont = false;
        enableSize = false;
    } else if (sel == 17) { // Editor default
        fgColour = theme.getDefault().foreground;
        bgColour = theme.getDefault().background;
        fontSize = theme.getDefault().fontSize;
        enableFont = false;
        enableStyle = false;
    }

    m_btnForeground->Enable(enableFg);
    m_btnBackground->Enable(enableBg);
    m_chFont->Enable(enableFont);
    m_chkBold->Enable(enableStyle);
    m_chkItalic->Enable(enableStyle);
    m_chkUnderline->Enable(enableStyle);
    m_spinFontSize->Enable(enableSize);

    if (fgColour.IsOk()) {
        m_btnForeground->SetBackgroundColour(fgColour);
    }
    if (bgColour.IsOk()) {
        m_btnBackground->SetBackgroundColour(bgColour);
    }

    m_chkBold->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Bold)) != 0);
    m_chkItalic->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Italic)) != 0);
    m_chkUnderline->SetValue((fontStyle & static_cast<int>(Theme::FontStyle::Underline)) != 0);
    m_spinFontSize->SetValue(fontSize);

    if (enableFont) {
        const auto fontIdx = m_chFont->FindString(fontName);
        m_chFont->SetSelection(fontIdx != wxNOT_FOUND ? fontIdx : 0);
    }
}

void SettingsDialog::storeTypeSelection(const int sel) {
    auto& theme = m_ctx.getTheme();
    const auto fgColour = m_btnForeground->GetBackgroundColour();
    const auto bgColour = m_btnBackground->GetBackgroundColour();
    int fontStyle = 0;
    if (m_chkBold->GetValue()) {
        fontStyle |= static_cast<int>(Theme::FontStyle::Bold);
    }
    if (m_chkItalic->GetValue()) {
        fontStyle |= static_cast<int>(Theme::FontStyle::Italic);
    }
    if (m_chkUnderline->GetValue()) {
        fontStyle |= static_cast<int>(Theme::FontStyle::Underline);
    }

    if (sel < 12) {
        auto& style = theme.getStyle(static_cast<Theme::ItemKind>(sel + 1));
        style.foreground = fgColour;
        style.background = bgColour;
        style.fontStyle = static_cast<Theme::FontStyle>(fontStyle);
        style.fontSize = m_spinFontSize->GetValue();
        if (m_chFont->GetSelection() > 0) {
            style.fontName = m_chFont->GetStringSelection();
        }
    } else if (sel == 12) {
        theme.getDefault().caretColour = fgColour;
    } else if (sel == 13) {
        theme.getLineNumber().foreground = fgColour;
        theme.getLineNumber().background = bgColour;
    } else if (sel == 14) {
        theme.getSelection().foreground = fgColour;
        theme.getSelection().background = bgColour;
    } else if (sel == 15) {
        theme.getBrace().foreground = fgColour;
        theme.getBrace().background = bgColour;
        theme.getBrace().fontStyle = static_cast<Theme::FontStyle>(fontStyle);
    } else if (sel == 16) {
        theme.getBadBrace().foreground = fgColour;
        theme.getBadBrace().background = bgColour;
        theme.getBadBrace().fontStyle = static_cast<Theme::FontStyle>(fontStyle);
    } else if (sel == 17) {
        theme.getDefault().foreground = fgColour;
        theme.getDefault().background = bgColour;
        theme.getDefault().fontSize = m_spinFontSize->GetValue();
    }
}

// -- Events --

void SettingsDialog::onThemeTypeSelected(wxCommandEvent& event) {
    storeTypeSelection(m_themeTypeOld);
    m_themeTypeOld = event.GetSelection();
    setTypeSelection(m_themeTypeOld);
}

void SettingsDialog::onThemeChanged(wxCommandEvent&) {
    auto sel = m_themeChoice->GetSelection();
    if (sel <= 0) {
        return;
    }
    auto themeName = m_themeChoice->GetStringSelection();
    auto& config = m_ctx.getConfig();
    m_ctx.getTheme().load(config.getIdePath() + themeName + ".fbt");
    setTypeSelection(m_themeTypeList->GetSelection());
}

void SettingsDialog::onSaveTheme(wxCommandEvent&) {
    storeTypeSelection(m_themeTypeList->GetSelection());

    if (m_themeChoice->GetSelection() == 0) {
        // Create new theme
        const auto& lang = m_ctx.getLang();
        wxTextEntryDialog dlg(this, lang[LangId::ThemeEnterName], lang[LangId::ThemeParametersTitle]);
        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
        auto name = dlg.GetValue().Trim().Trim(false).Lower();
        if (name.empty()) {
            return;
        }
        auto path = m_ctx.getConfig().getIdePath() + name + ".fbt";
        m_ctx.getTheme().load(path); // create by loading (will be empty)
        m_themeChoice->Append(name);
        m_themeChoice->SetStringSelection(name);
    }

    m_ctx.getTheme().save();
}

void SettingsDialog::onForegroundColor(wxCommandEvent&) {
    wxColourData data;
    data.SetColour(m_btnForeground->GetBackgroundColour());
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        m_btnForeground->SetBackgroundColour(dlg.GetColourData().GetColour());
        m_btnForeground->Refresh();
    }
}

void SettingsDialog::onBackgroundColor(wxCommandEvent&) {
    wxColourData data;
    data.SetColour(m_btnBackground->GetBackgroundColour());
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        m_btnBackground->SetBackgroundColour(dlg.GetColourData().GetColour());
        m_btnBackground->Refresh();
    }
}

void SettingsDialog::onKeywordGroupChanged(wxCommandEvent& event) {
    // Save current group
    m_keywordGroups[static_cast<size_t>(m_keywordGroupOld)] = m_textKeywords->GetValue();
    m_keywordGroupOld = event.GetSelection();
    m_textKeywords->SetValue(m_keywordGroups[static_cast<size_t>(m_keywordGroupOld)]);
}

void SettingsDialog::onCompilerBrowse(wxCommandEvent&) {
    wxFileDialog dlg(this, "Select compiler", "", "",
#ifdef __WXMSW__
        "FreeBASIC (fbc.exe)|fbc.exe|All programs (*.exe)|*.exe",
#else
        "All files (*)|*",
#endif
        wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_textCompilerPath->SetValue(dlg.GetPath());
    }
}

void SettingsDialog::onHelpBrowse(wxCommandEvent&) {
    wxFileDialog dlg(this, "Select help file", "", "",
        m_ctx.getLang()[LangId::SettingsHelpFileFilter] + "|*.chm",
        wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_textHelpFile->SetValue(dlg.GetPath());
    }
}

// -- Apply --

void SettingsDialog::applyChanges() {
    auto& config = m_ctx.getConfig();

    // General
    config.setAutoIndent(m_chkAutoIndent->GetValue());
    config.setIndentGuide(m_chkIndentGuides->GetValue());
    config.setWhiteSpace(m_chkWhiteSpace->GetValue());
    config.setDisplayEOL(m_chkLineEndings->GetValue());
    config.setBraceHighlight(m_chkBraceHighlight->GetValue());
    config.setSyntaxHighlight(m_chkSyntaxHighlight->GetValue());
    config.setLineNumbers(m_chkLineNumbers->GetValue());
    config.setLongLine(m_chkRightMargin->GetValue());
    config.setFolderMargin(m_chkFoldMargin->GetValue());
    config.setSplashScreen(m_chkSplashScreen->GetValue());
    config.setEdgeColumn(m_spinEdgeColumn->GetValue());
    config.setTabSize(m_spinTabSize->GetValue());

    if (m_chLanguage->GetSelection() != wxNOT_FOUND) {
        config.setLanguage(m_chLanguage->GetStringSelection());
    }

    // Theme — store current selection before applying
    storeTypeSelection(m_themeTypeList->GetSelection());
    if (m_themeChoice->GetSelection() > 0) {
        config.setThemeFile(m_themeChoice->GetStringSelection() + ".fbt");
    }

    // Keywords — save current group first
    m_keywordGroups[static_cast<size_t>(m_keywordGroupOld)] = m_textKeywords->GetValue();
    auto& keywords = m_ctx.getKeywords();
    for (int idx = 0; idx < 4; idx++) {
        keywords.setGroup(idx, m_keywordGroups[static_cast<size_t>(idx)]);
    }
    keywords.save();

    // Compiler
    config.setCompilerPath(m_textCompilerPath->GetValue());
    config.setCompileCommand(m_textCompileCommand->GetValue());
    config.setRunCommand(m_textRunCommand->GetValue());
    config.setHelpFile(m_textHelpFile->GetValue());

    // Save config
    config.save();

    // Reapply settings to all open editors
    auto& docManager = m_ctx.getDocumentManager();
    for (size_t idx = 0; idx < docManager.getCount(); idx++) {
        if (auto* notebook = m_ctx.getUIManager().getNotebook()) {
            if (auto* page = notebook->GetPage(idx)) {
                if (auto* editor = dynamic_cast<Editor*>(page)) {
                    editor->applySettings();
                }
            }
        }
    }
}
