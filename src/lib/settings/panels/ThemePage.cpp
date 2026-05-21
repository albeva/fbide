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
                theme.set## NAME(getThemeValue<Theme::TYPE>(v)); break;
            DEFINE_THEME_EXTRA_PROPERTY(EXTRA_CASE)
        #undef EXTRA_CASE
        // clang-format on
    default:
        std::unreachable();
    }
}

auto categoryTreeLayout() -> std::vector<ThemePage::TreeNode> {
    using SC = SettingsCategory;
    // clang-format off
    return {
        { "default", "Default", SC::Default, {
            { "comments",          "Comments",           SC::Comment,          {} },
            { "multilineComments", "Multiline comments", SC::MultilineComment, {} },
            { "identifier",        "Identifier",         SC::Identifier,       {} },
            { "number",            "Number",             SC::Number,           {} },
            { "string",            "String",             SC::String, {
                { "unterminated", "Unterminated", SC::StringOpen, {} },
            } },
            { "operator",          "Operator",           SC::Operator,         {} },
            { "label",             "Label",              SC::Label,            {} },
            { "error",             "Error",              SC::Error,            {} },
            { "keywords", "Keywords", std::nullopt, {
                { "core",              "Core",      SC::Keywords,         {} },
                { "types",             "Types",     SC::KeywordTypes,     {} },
                { "operators",         "Operators", SC::KeywordOperators, {} },
                { "defines",           "Defines",   SC::KeywordConstants, {} },
                { "library",           "Library",   SC::KeywordLibrary,   {} },
                { "custom",            "Custom",    SC::KeywordCustom,    {} },
            } },
            { "margins", "Margins", std::nullopt, {
                { "lineNumbers", "Line numbers", SC::LineNumber, {} },
                { "fold",        "Fold",         SC::FoldMargin, {} },
            } },
            { "selection", "Selection", SC::Selection, {} },
            { "brace", "Brace", std::nullopt, {
                { "match",    "Match",    SC::Brace,    {} },
                { "mismatch", "Mismatch", SC::BadBrace, {} },
            } },
        } },
        { "asm", "Asm", std::nullopt, {
            { "instructions", "Instructions", SC::KeywordAsm1, {} },
            { "registers",    "Registers",    SC::KeywordAsm2, {} },
        } },
        { "preprocessor", "Preprocessor", SC::Preprocessor, {
            { "directives",   "Directives", SC::KeywordPP,    {} },
            { "ppIdentifier", "Identifier", SC::IdentifierPP, {} },
            { "ppNumber",     "Number",     SC::NumberPP,     {} },
            { "ppString",     "String",     SC::StringPP,     {} },
            { "ppOperator",   "Operator",   SC::OperatorPP,   {} },
        } },
    };
    // clang-format on
}

} // namespace

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// clang-format off
wxBEGIN_EVENT_TABLE(ThemePage, Panel)
    EVT_CHOICE             (ThemePage::ID_THEME_CHOICE,   ThemePage::onSelectTheme)
    EVT_BUTTON             (ThemePage::ID_SAVE_THEME,     ThemePage::onSaveTheme)
    EVT_TREE_SEL_CHANGING  (ThemePage::ID_CATEGORY_TREE,  ThemePage::onCategorySelChanging)
    EVT_TREE_SEL_CHANGED   (ThemePage::ID_CATEGORY_TREE,  ThemePage::onCategorySelChanged)
wxEND_EVENT_TABLE()
// clang-format on

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ThemePage::ThemePage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent)
, m_theme(getContext().getTheme())
, m_activeTheme(m_theme.getPath())
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

auto ThemePage::tr(const wxString& path, const wxString& def) const -> wxString {
    return m_tr.get_or(path, def.IsEmpty() ? path : def);
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

    // Pre-load the Default row before triggering the tree's selection
    // event. SelectItem fires SEL_CHANGED → saveCategory() → loadCategory()
    // — saveCategory reads from the picker widgets, so they need to hold
    // Default's colours first; otherwise it clobbers Default with the
    // picker's null/empty initial state. Layout guarantees the first
    // top-level child of the (hidden) root is the Default node.
    m_selectedRow = +SettingsCategory::Default;
    loadCategory();
    wxTreeItemIdValue cookie;
    if (const auto first = m_typeTree->GetFirstChild(m_typeTree->GetRootItem(), cookie); first.IsOk()) {
        m_typeTree->SelectItem(first);
    }
}

void ThemePage::createTopRow() {
    hbox(tr("name"), { .center = true, .border = 0 }, [&] {
        m_themeFiles = getContext().getConfigManager().getAllThemes();
        wxArrayString names;
        names.reserve(m_themeFiles.size() + 1);

        names.Add(tr("createNew"));
        for (const auto& path : m_themeFiles) {
            auto name = wxFileName(path).GetName();
            if (path.EndsWith(".fbt")) {
                name += " (fbide " + Version::oldFbide().asString() + ")";
            }
            names.Add(name);
        }

        m_themeChoice = choice(names, { .proportion = 1, .expand = false }, ID_THEME_CHOICE);

        // find currently active theme selection
        const auto index = [&] -> int {
            const auto iter = std::ranges::find(m_themeFiles, m_activeTheme);
            if (iter == m_themeFiles.end()) {
                m_activeTheme = "";
                return 0;
            }
            return static_cast<int>(std::ranges::distance(m_themeFiles.begin(), iter)) + FILE_OFFSET;
        }();
        m_themeChoice->SetSelection(index);

        button(tr("save"), { .expand = false }, ID_SAVE_THEME);
    });
}

void ThemePage::addTreeNode(const wxTreeItemId parent, const std::vector<TreeNode>& nodes) {
    for (const auto& node : nodes) {
        const auto label = tr("categories." + node.labelKey, node.fallbackLabel);
        const auto id = m_typeTree->AppendItem(parent, label);
        if (node.category) {
            m_treeCategories.emplace(id.GetID(), *node.category);
            m_typeTree->SetItemBold(id, true);
        }
        if (!node.children.empty()) {
            addTreeNode(id, node.children);
        }
    }
}

void ThemePage::createCategoryList() {
    m_typeTree = make_unowned<wxTreeCtrl>(
        currentParent(), ID_CATEGORY_TREE,
        wxDefaultPosition, wxSize(180, 320),
        wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT | wxTR_FULL_ROW_HIGHLIGHT
    );

    const auto root = m_typeTree->AddRoot("(root)");
    addTreeNode(root, categoryTreeLayout());
    m_typeTree->ExpandAll();
    m_selectedRow = +SettingsCategory::Default;

    add(m_typeTree, {});
}

void ThemePage::createLeftPanel() {
    vbox({ .proportion = 2, .border = 0 }, [this] {
        const auto addPicker = [&](const wxString& labelText, const wxString& tooltip = {}) -> Unowned<ColorPicker> {
            auto picker = make_unowned<ColorPicker>(currentParent(), m_theme, m_tr, labelText, tooltip);
            picker->create();
            add(picker);
            return picker;
        };

        const auto inheritTip = tr("inheritColor");
        m_fgPicker = addPicker(tr("foreground"), inheritTip);
        spacer();
        m_bgPicker = addPicker(tr("background"), inheritTip);
        spacer();
        m_separatorPicker = addPicker(tr("separator"));
        spacer();

        m_lblFont = text(tr("font"), {});
        m_fontChoice = make_unowned<wxChoice>(currentParent(), wxID_ANY);
        m_fontChoice->Append(getAllFixedWidthFonts());
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

void ThemePage::onSelectTheme(wxCommandEvent& event) {
    const auto index = static_cast<std::size_t>(event.GetSelection());
    if (index > 0) {
        m_activeTheme = m_themeFiles[index - FILE_OFFSET];
    } else {
        m_activeTheme = "";
    }

    updateTitle();
    if (not isUnsavedNewTheme()) {
        m_theme = {};
        m_theme.load(m_activeTheme);
        loadCategory();
    }
}

void ThemePage::onSaveTheme(wxCommandEvent&) {
    saveCategory();

    if (isUnsavedNewTheme()) {
        saveNewTheme(false);
        updateTitle();
    }

    // Route saves through `themesWriteDir()` — under READONLY this is
    // `<UserDataDir>/themes/`, so edits to a bundled theme land in the
    // user copy (which then shadows the bundle via the two-dir merge
    // in `getAllThemes`). Same-name file in portable mode is a no-op
    // redirect.
    auto& cm = getContext().getConfigManager();
    const wxString writeDir = cm.themesWriteDir();
    wxFileName::Mkdir(writeDir, 0755, wxPATH_MKDIR_FULL);
    const wxString target = writeDir + wxFILE_SEP_PATH + wxFileName(m_theme.getPath()).GetFullName();
    m_theme.save(target);

    const auto currentThemeName = getContext().getTheme().getPath();
    if (m_activeTheme == currentThemeName) {
        getContext().getTheme() = m_theme;
        syncActiveThemeConfig();
        getContext().getUIManager().updateSettings();
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

    const wxString writeDir = getContext().getConfigManager().themesWriteDir();
    const wxFileName path(writeDir / name + ".ini");
    if (not path.IsOk()) {
        wxMessageBox(wxString::Format(tr("invalidFilename"), path.GetFullPath()));
        return;
    }

    if (path.Exists()) {
        wxMessageBox(wxString::Format(tr("fileExists"), path.GetFullPath()));
        return;
    }

    wxFileName::Mkdir(writeDir, 0755, wxPATH_MKDIR_FULL);
    m_theme.save(path.GetAbsolutePath());
    m_themeChoice->Append(name);
    m_themeChoice->SetStringSelection(name);
    m_activeTheme = m_theme.getPath();

    if (setActive) {
        getContext().getTheme() = m_theme;
        syncActiveThemeConfig();
    }
}

// ---------------------------------------------------------------------------
// Category handling
// ---------------------------------------------------------------------------

void ThemePage::onCategorySelChanging(wxTreeEvent& event) {
    // Veto selection of folder nodes (Keywords / Margins / Brace / Asm).
    // wx fires SEL_CHANGED only when the new selection sticks, so vetoing
    // here prevents flicker and keeps `m_selectedRow` aligned with the
    // last real (selectable) category.
    const auto newItem = event.GetItem();
    if (!newItem.IsOk()) {
        return;
    }
    if (!m_treeCategories.contains(newItem.GetID())) {
        event.Veto();
    }
}

void ThemePage::onCategorySelChanged(wxTreeEvent& event) {
    const auto item = event.GetItem();
    if (!item.IsOk()) {
        return;
    }
    const auto it = m_treeCategories.find(item.GetID());
    if (it == m_treeCategories.end()) {
        return;
    }
    saveCategory();
    m_selectedRow = +it->second;
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
    m_themeBox->GetStaticBox()->SetLabel(m_themeChoice->GetStringSelection());
}
