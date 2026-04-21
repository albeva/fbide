//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "ThemePage.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
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

// ---------------------------------------------------------------------------
// Theme category helpers
// ---------------------------------------------------------------------------

template<typename T>
auto getThemeValue(const Theme::Entry& entry) -> const T& {
    if constexpr (std::is_same_v<T, Theme::Entry>) {
        return entry;
    } else if constexpr (std::is_same_v<T, Theme::Colors>) {
        return entry.colors;
    } else {
        std::unreachable();
    }
};

void writeCategory(Theme& theme, const SettingsCategory cat, const Theme::Entry& v) {
    if (isSyntaxCategory(cat)) {
        theme.set(static_cast<ThemeCategory>(+cat), v);
        return;
    }

    switch (cat) {
        // clang-format off
        // extra properties
        #define EXTRA_CASE(NAME, unused, TYPE) \
            case SettingsCategory::NAME: \
                theme.set##NAME(getThemeValue<Theme::TYPE>(v)); break;
            DEFINE_THEME_EXTRA_PROPERTY(EXTRA_CASE)
        #undef EXTRA_CASE
        // clang-format on
    default:
        std::unreachable();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// clang-format off
wxBEGIN_EVENT_TABLE(ThemePage, Panel)
    EVT_CHOICE  (ThemePage::ID_THEME_CHOICE,    ThemePage::onSelectTheme)
    EVT_BUTTON  (ThemePage::ID_SAVE_THEME,      ThemePage::onSaveTheme)
    EVT_LISTBOX (ThemePage::ID_CATEGORY_LIST,   ThemePage::onSelectCategory)
wxEND_EVENT_TABLE()
// clang-format on

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ThemePage::ThemePage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_activeTheme(wxFileName(ctx.getTheme().getPath()).GetName())
, m_theme(getContext().getTheme())
, m_tr(getContext().getConfigManager().locale().at("dialogs.settings.themes")) {}

auto ThemePage::getAllFixedWidthFonts() -> std::vector<wxString> {
    wxFontEnumerator fontEnum;
    auto fontList = fontEnum.GetFacenames(wxFONTENCODING_SYSTEM, true);
    fontList.Sort();
    return { fontList.begin(), fontList.end() };
}

// ---------------------------------------------------------------------------
// Apply theme settings
// ---------------------------------------------------------------------------

void ThemePage::apply() {
    saveCategory();

    if (isUnsavedNewTheme()) {
        saveNewTheme(true);
    } else {
        getContext().getTheme() = m_theme;
        syncActiveThemeConfig();
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
    SetSizerAndFit(currentSizer());

    updateTitle();
    loadCategory();
}

void ThemePage::createTopRow() {
    hbox(tr("name"), { .center = true, .border = 0 }, [&] {
        const auto themes = getContext().getConfigManager().getAllThemes();
        wxArrayString names;
        names.Add(tr("createNew"));
        for (const auto& path : themes) {
            names.Add(wxFileName(path).GetName());
        }

        // Use the non-ref choice overload — Layout's value-bound overload
        // installs its own EVT_CHOICE lambda that consumes the event and
        // blocks our event-table handler (onSelectTheme) from firing.
        m_themeChoice = choice(names, { .proportion = 1, .expand = false }, ID_THEME_CHOICE);
        const auto sel = m_themeChoice->FindString(m_activeTheme);
        m_themeChoice->SetSelection(sel != wxNOT_FOUND ? sel : 0);

        button(tr("save"), { .expand = false }, ID_SAVE_THEME);
    });
}

void ThemePage::createCategoryList() {
    wxArrayString displayNames;
    displayNames.reserve(kSettingsCategoryCount);

    for (const auto cat : kSettingsCategories) {
        const auto key = "categories." + lowerFirst(getSettingsCategoryName(cat));
        auto label = tr(key);
        if (label.empty()) {
            label = getSettingsCategoryName(cat);
        }
        displayNames.Add(label);
    }

    m_typeList = make_unowned<wxListBox>(currentParent(), ID_CATEGORY_LIST, wxDefaultPosition, wxSize(160, 240), displayNames);
    m_selectedRow = 0;
    m_typeList->SetSelection(m_selectedRow);
    add(m_typeList, {});
}

void ThemePage::createLeftPanel() {
    const auto inheritTip = tr("inheritColor");

    auto addPicker = [&](const wxString& labelText, const wxString& tooltip = {}) -> Unowned<ColorPicker> {
        auto picker = make_unowned<ColorPicker>(currentParent(), m_theme, m_tr, labelText, tooltip);
        picker->create();
        add(picker);
        return picker;
    };

    vbox({ .proportion = 2, .border = 0 }, [&] {
        m_fgPicker = addPicker(tr("foreground"), inheritTip);
        spacer();
        m_bgPicker = addPicker(tr("background"), inheritTip);
        spacer();
        m_separatorPicker = addPicker(tr("separator"));
        spacer();

        m_lblFont = text(tr("font"), {});
        m_fontChoice = make_unowned<wxChoice>(currentParent(), wxID_ANY);
        auto fonts = getAllFixedWidthFonts();
        fonts.insert(fonts.begin(), "");
        wxArrayString fontArr;
        for (const auto& f : fonts) {
            fontArr.Add(f);
        }
        m_fontChoice->Append(fontArr);
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

// ---------------------------------------------------------------------------
// Theme selection
// ---------------------------------------------------------------------------

void ThemePage::onSelectTheme(wxCommandEvent&) {
    m_activeTheme = m_themeChoice->GetStringSelection();
    updateTitle();
    if (not isUnsavedNewTheme()) {
        m_theme = {};
        m_theme.load(getContext().getConfigManager().absolute("themes/" + m_activeTheme + ".ini"));
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

    const auto currentThemeName = wxFileName(getContext().getTheme().getPath()).GetName();
    if (m_activeTheme == currentThemeName) {
        getContext().getTheme() = m_theme;
        syncActiveThemeConfig();
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

    const wxFileName path(getContext().getConfigManager().getIdeDir() + "themes/" + name + ".ini");
    if (not path.IsOk() or path.Exists()) {
        wxLogWarning("Unable to save theme as %s", path.GetAbsolutePath());
        return;
    }

    m_theme.save(path.GetAbsolutePath());

    m_themeChoice->Append(name);
    m_themeChoice->SetStringSelection(name);
    m_activeTheme = name;

    if (setActive) {
        getContext().getTheme() = m_theme;
        syncActiveThemeConfig();
    }
}

// ---------------------------------------------------------------------------
// Category handling
// ---------------------------------------------------------------------------

void ThemePage::onSelectCategory(wxCommandEvent& event) {
    saveCategory();
    m_selectedRow = event.GetSelection();
    loadCategory();
}

void ThemePage::loadCategory() {
    const auto cat = static_cast<SettingsCategory>(m_selectedRow);
    const auto cap = capabilityOf(cat);
    const auto view = readCategory(m_theme, cat);
    const auto& defaultColors = m_theme.get(ThemeCategory::Default).colors;
    const bool isDefault = cat == SettingsCategory::Default;

    applyCapability();

    m_fgPicker->setColors(view.colors.foreground, isDefault ? wxNullColour : defaultColors.foreground);
    m_bgPicker->setColors(view.colors.background, isDefault ? wxNullColour : defaultColors.background);

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
    if (cap.separator) {
        m_separatorPicker->setColors(m_theme.getSeparator());
    }

    GetSizer()->Layout();
}

void ThemePage::saveCategory() {
    const auto cat = static_cast<SettingsCategory>(m_selectedRow);
    const auto cap = capabilityOf(cat);

    Theme::Entry view;
    if (cap.foreground) {
        view.colors.foreground = m_fgPicker->getColor();
    }
    if (cap.background) {
        view.colors.background = m_bgPicker->getColor();
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
    if (cap.separator) {
        m_theme.setSeparator(m_separatorPicker->getColor());
    }
}

void ThemePage::applyCapability() {
    const auto cat = static_cast<SettingsCategory>(m_selectedRow);
    const auto cap = capabilityOf(cat);

    m_fgPicker->Show(cap.foreground);
    m_bgPicker->Show(cap.background);
    m_separatorPicker->Show(cap.separator);

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
