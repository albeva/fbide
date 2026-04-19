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
#include "config/ConfigManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {

/// Lowercase the first character — used for locale key lookup
/// ("LineNumber" → "lineNumber").
auto lowerFirst(const std::string_view name) -> wxString {
    wxString out = wxString::FromAscii(name.data(), name.size());
    if (not out.empty()) {
        out[0] = wxTolower(out[0]);
    }
    return out;
}

/// Read-only snapshot of theme data for category.
struct CategoryView final {
    Theme::Colors colors;
    bool bold = false;
    bool italic = false;
    bool underlined = false;
};

auto readCategory(const Theme& theme, const SettingsCategory cat) -> CategoryView {
    if (isSyntaxCategory(cat)) {
        const auto& e = theme.get(static_cast<ThemeCategory>(+cat));
        return { e.colors, e.bold, e.italic, e.underlined };
    }
    switch (cat) {
    case SettingsCategory::LineNumber:
        return { theme.getLineNumber() };
    case SettingsCategory::Selection:
        return { theme.getSelection() };
    case SettingsCategory::Brace: {
        const auto& e = theme.getBrace();
        return { e.colors, e.bold, e.italic, e.underlined };
    }
    case SettingsCategory::BadBrace: {
        const auto& e = theme.getBadBrace();
        return { e.colors, e.bold, e.italic, e.underlined };
    }
    default:
        return {};
    }
}

void writeCategory(Theme& theme, const SettingsCategory cat, const CategoryView& v) {
    if (isSyntaxCategory(cat)) {
        theme.set(static_cast<ThemeCategory>(+cat), Theme::Entry {
            .colors = v.colors,
            .bold = v.bold,
            .italic = v.italic,
            .underlined = v.underlined,
        });
        return;
    }
    switch (cat) {
    case SettingsCategory::LineNumber:
        theme.setLineNumber(v.colors);
        break;
    case SettingsCategory::Selection:
        theme.setSelection(v.colors);
        break;
    case SettingsCategory::Brace:
        theme.setBrace({ .colors = v.colors, .bold = v.bold, .italic = v.italic, .underlined = v.underlined });
        break;
    case SettingsCategory::BadBrace:
        theme.setBadBrace({ .colors = v.colors, .bold = v.bold, .italic = v.italic, .underlined = v.underlined });
        break;
    default:
        break;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ThemePage::ThemePage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_activeTheme(getConfig().getTheme())
, m_theme(getContext().getTheme())
, m_tr(getContext().getConfigManager().locale().at("dialogs.settings.themes")) {}

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

auto ThemePage::tr(const wxString& path) const -> wxString {
    return m_tr.get_or(path, path);
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
    hbox(tr("name"), { .center = true, .border = 0 }, [&] {
        auto themes = getConfig().getAllThemes();
        themes.insert(themes.begin(), tr("createNew"));

        m_themeChoice = choice(m_activeTheme, themes, { .proportion = 1, .expand = false });
        m_themeChoice->Bind(wxEVT_CHOICE, &ThemePage::onSelectTheme, this);

        const auto save = button(tr("save"), { .expand = false });
        save->Bind(wxEVT_BUTTON, &ThemePage::onSaveTheme, this);
    });
}

void ThemePage::createCategoryList() {
    struct Row {
        SettingsCategory cat;
        wxString         label;
    };
    std::array<Row, kSettingsCategoryCount - 1> rows;
    wxArrayString displayNames;
    displayNames.reserve(kSettingsCategoryCount);

    std::size_t index = 0;
    for (const auto cat : kSettingsCategories) {
        const auto key = "categories." + lowerFirst(getSettingsCategoryName(cat));
        auto label = tr(key);
        if (label.empty()) {
            label = getSettingsCategoryName(cat);
        }
        if (cat == SettingsCategory::Default) {
            displayNames.Add(std::move(label));
            m_categoryOrder[0] = cat;
        } else {
            rows[index].cat = cat;
            rows[index].label = std::move(label);
            index++;
        }
    }

    std::ranges::sort(rows, [](const Row& a, const Row& b) {
        return a.label.CmpNoCase(b.label) < 0;
    });

    for (std::size_t i = 1; i < rows.size(); i++) {
        displayNames.Add(rows[i].label);
        m_categoryOrder[i] = rows[i].cat;
    }

    m_typeList = make_unowned<wxListBox>(currentParent(), wxID_ANY, wxDefaultPosition, wxSize(160, 240), displayNames);
    m_selectedRow = 0;
    m_typeList->SetSelection(m_selectedRow);

    m_typeList->Bind(wxEVT_LISTBOX, &ThemePage::onSelectCategory, this);
    add(m_typeList, {});
}

void ThemePage::createLeftPanel() {
    vbox({ .proportion = 2, .border = 0 }, [&] {
        m_lblFg = text(tr("foreground"), {});
        m_btnFg = button(wxString {});
        m_btnFg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnFg); });
        connect(m_lblFg, m_btnFg);

        spacer();

        m_lblBg = text(tr("background"), {});
        m_btnBg = button(wxString {});
        m_btnBg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onColorButton(m_btnBg); });
        connect(m_lblBg, m_btnBg);

        spacer();

        m_lblFont = text(tr("font"), {});
        m_fontChoice = make_unowned<wxChoice>(currentParent(), wxID_ANY);
        auto fonts = Config::getAllFixedWidthFonts();
        fonts.insert(fonts.begin(), "");
        m_fontChoice->Append(fonts);
        add(m_fontChoice);
        connect(m_lblFont, m_fontChoice);
    });
}

void ThemePage::createRightPanel() {
    vbox({ .proportion = 1, .border = 0 }, [&] {
        m_fontOptionsLabel = text(tr("fontOptions"), {});

        m_chkBold = checkBox(tr("bold"));
        m_chkItalic = checkBox(tr("italic"));
        m_chkUnderline = checkBox(tr("underline"));

        spacer();

        m_lblFontSize = text(tr("fontSize"), {});
        m_spinFontSize = make_unowned<wxSpinCtrl>(
            currentParent(), wxID_ANY, wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 8, 64, 12
        );
        add(m_spinFontSize);
        connect(m_lblFontSize, m_spinFontSize);
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
        m_theme = {};
        m_theme.load(getConfig().resolvePath(m_activeTheme + ".ini"));
        loadCategory();
    }
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    saveCategory();

    if (isUnsavedNewTheme()) {
        saveNewTheme(false);
        updateTitle();
    }

    m_theme.save();
    syncActiveThemeConfig();

    if (m_activeTheme == getConfig().getTheme()) {
        getContext().getTheme() = m_theme;
        getContext().getUIManager().updateEditorSettigs();
    }
}

void ThemePage::saveNewTheme(const bool setActive) {
    if (not isUnsavedNewTheme()) {
        return;
    }

    wxTextEntryDialog dlg(this, tr("enterName"), tr("nameDialogTitle"));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto name = dlg.GetValue().Trim().Trim(false).Lower();
    if (name.empty()) {
        return;
    }

    const wxFileName path = getConfig().getAppSettingsPath() + name + ".ini";
    if (not path.IsOk() or path.Exists()) {
        wxLogWarning("Unable to save theme as %s", path.GetAbsolutePath());
        return;
    }

    m_theme.save(path.GetAbsolutePath());

    m_themeChoice->Append(name);
    m_themeChoice->SetStringSelection(name);
    m_activeTheme = name;

    if (setActive) {
        getConfig().setTheme(name);
        getContext().getTheme() = m_theme;
    }
}

// ---------------------------------------------------------------------------
// Category handling
// ---------------------------------------------------------------------------

void ThemePage::onSelectCategory(const wxCommandEvent& event) {
    saveCategory();
    m_selectedRow = event.GetSelection();
    loadCategory();
}

void ThemePage::loadCategory() {
    const auto cat = m_categoryOrder[static_cast<std::size_t>(m_selectedRow)];
    const auto cap = capabilityOf(cat);
    const auto view = readCategory(m_theme, cat);

    applyCapability();

    if (view.colors.foreground.IsOk()) {
        m_btnFg->SetBackgroundColour(view.colors.foreground);
        m_btnFg->Refresh();
    }
    if (view.colors.background.IsOk()) {
        m_btnBg->SetBackgroundColour(view.colors.background);
        m_btnBg->Refresh();
    }

    m_chkBold->SetValue(view.bold);
    m_chkItalic->SetValue(view.italic);
    m_chkUnderline->SetValue(view.underlined);

    if (cap.font) {
        const auto idx = m_fontChoice->FindString(m_theme.getFont());
        m_fontChoice->SetSelection(idx != wxNOT_FOUND ? idx : 0);
    }
    if (cap.fontSize) {
        m_spinFontSize->SetValue(m_theme.getFontSize() > 0 ? m_theme.getFontSize() : 12);
    }

    GetSizer()->Layout();
    Update();
}

void ThemePage::saveCategory() {
    const auto cat = m_categoryOrder[static_cast<std::size_t>(m_selectedRow)];
    const auto cap = capabilityOf(cat);

    CategoryView view;
    if (cap.foreground) {
        view.colors.foreground = m_btnFg->GetBackgroundColour();
    }
    if (cap.background) {
        view.colors.background = m_btnBg->GetBackgroundColour();
    }
    if (cap.style) {
        view.bold = m_chkBold->GetValue();
        view.italic = m_chkItalic->GetValue();
        view.underlined = m_chkUnderline->GetValue();
    }
    writeCategory(m_theme, cat, view);

    // Font + size are editor-wide; only written when capability has them (Default).
    if (cap.font && m_fontChoice->GetSelection() > 0) {
        m_theme.setFont(m_fontChoice->GetStringSelection());
    }
    if (cap.fontSize) {
        m_theme.setFontSize(m_spinFontSize->GetValue());
    }
}

void ThemePage::applyCapability() {
    const auto cat = m_categoryOrder[static_cast<std::size_t>(m_selectedRow)];
    const auto cap = capabilityOf(cat);

    m_lblFg->Show(cap.foreground);
    m_btnFg->Show(cap.foreground);
    m_lblBg->Show(cap.background);
    m_btnBg->Show(cap.background);

    m_chkBold->Show(cap.style);
    m_chkItalic->Show(cap.style);
    m_chkUnderline->Show(cap.style);
    if (not cap.style) {
        m_fontOptionsLabel->Hide();
    }

    m_lblFont->Show(cap.font);
    m_fontChoice->Show(cap.font);
    m_lblFontSize->Show(cap.fontSize);
    m_spinFontSize->Show(cap.fontSize);

    Layout();
}

void ThemePage::syncActiveThemeConfig() {
    auto& cm = getContext().getConfigManager();
    cm.config()["theme"] = cm.relative(m_theme.getPath());
    cm.save(ConfigManager::Category::Config);
}

void ThemePage::updateTitle() {
    m_themeBox->GetStaticBox()->SetLabel(m_activeTheme.Capitalize());
}
