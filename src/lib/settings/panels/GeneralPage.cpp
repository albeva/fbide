//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "GeneralPage.hpp"
#include "app/App.hpp"
#include "app/Appearance.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/TextEncoding.hpp"
using namespace fbide;

namespace {
auto currentLocaleFileName(const Value& cfg) -> wxString {
    return wxFileName(cfg.get_or("locale", "")).GetFullName();
}

auto encodingChoices() -> wxArrayString {
    wxArrayString names;
    for (const auto value : TextEncoding::all) {
        names.Add(wxString::FromUTF8(TextEncoding { value }.toString()));
    }
    return names;
}

auto eolModeChoices() -> wxArrayString {
    wxArrayString names;
    for (const auto value : EolMode::all) {
        names.Add(wxString::FromUTF8(EolMode { value }.toString()));
    }
    return names;
}

/// One entry in the language dropdown: the file we persist back to
/// config (`locales/<file>`) plus the human-readable display name we
/// pull from the file's `name=` header (falling back to the filename
/// when the file is unreadable or the key is missing).
struct LanguageOption {
    wxString fileName;
    wxString displayName;
};

/// Read the `name=` field from a locale `.ini` file. Returns empty
/// when the file can't be opened or has no `name`.
auto readLocaleName(const wxString& path) -> wxString {
    wxFFileInputStream stream(path);
    if (!stream.IsOk()) {
        return {};
    }
    const wxFileConfig cfg(stream, wxConvUTF8);
    wxString name;
    cfg.Read("name", &name);
    return name;
}

/// Build the sorted list of locale options for the dropdown.
auto loadLanguageOptions(const Context& ctx) -> std::vector<LanguageOption> {
    std::vector<LanguageOption> result;
    for (const auto& path : ctx.getConfigManager().getAllLanguages()) {
        const auto fileName = wxFileName(path).GetFullName();
        auto displayName = readLocaleName(path);
        if (displayName.IsEmpty()) {
            displayName = fileName;
        }
        result.push_back({ .fileName = fileName, .displayName = displayName });
    }
    std::ranges::sort(result, [](const auto& a, const auto& b) {
        return a.displayName.CmpNoCase(b.displayName) < 0;
    });
    return result;
}

} // namespace

GeneralPage::GeneralPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& cfg = getContext().getConfigManager().config();
    const auto& editor = cfg.at("editor");
    m_autoIndent = editor.get_or("autoIndent", true);
    m_transformKeywords = editor.get_or("transformKeywords", true);
    m_indentGuide = editor.get_or("indentGuide", false);
    m_showWhiteSpaces = editor.get_or("whiteSpace", false);
    m_showLineEndings = editor.get_or("displayEOL", false);
    m_braceHighlight = editor.get_or("braceHighlight", true);
    m_highlightOccurrences = editor.get_or("highlightOccurrences", true);
    m_codeCompletion = editor.get_or("codeCompletion", true);
    m_syntaxHighlight = editor.get_or("syntaxHighlight", true);
    m_showLineNumbers = editor.get_or("lineNumbers", true);
    m_showRightMargin = editor.get_or("longLine", false);
    m_foldMargin = editor.get_or("folderMargin", false);
    m_changeTracking = editor.get_or("changeTracking", true);
    m_autoReload = editor.get_or("autoReload", true);
    m_edgeColumn = editor.get_or("edgeColumn", 80);
    m_tabSize = editor.get_or("tabSize", 4);
    m_encoding = editor.get_or("encoding", "UTF-8");
    m_eolMode = editor.get_or("eolMode", "LF");
    m_splashScreen = cfg.get_or("general.splashScreen", true);
    m_configurationInStatusBar = cfg.get_or("commands.configurationInStatusBar", false);
    m_checkUpdatesOnLoad = cfg.get_or("update.checkOnStartup", true);
    m_language = currentLocaleFileName(cfg);
    m_appearance = cfg.get_or("appearance", "");
}

void GeneralPage::create() {
    hbox(tr("dialogs.settings.general.editorSettings"), { .margin = false }, [&] {
        vbox({ .proportion = 1, .margin = false }, [&] {
            checkBox(m_syntaxHighlight, tr("dialogs.settings.general.syntaxHighlight"));
            checkBox(m_autoIndent, tr("dialogs.settings.general.autoIndent"));
            checkBox(m_transformKeywords, tr("dialogs.settings.general.transformKeywords"));
            checkBox(m_indentGuide, tr("dialogs.settings.general.indentGuides"));
            checkBox(m_showWhiteSpaces, tr("dialogs.settings.general.whitespace"));
            checkBox(m_showLineEndings, tr("dialogs.settings.general.lineEndings"));
            checkBox(m_braceHighlight, tr("dialogs.settings.general.braceHighlight"));
            checkBox(m_highlightOccurrences, tr("dialogs.settings.general.highlightOccurrences"));
            checkBox(m_codeCompletion, tr("dialogs.settings.general.codeCompletion"));
            spinCtrl(m_edgeColumn, tr("dialogs.settings.general.rightMarginWidth"), 1, 200, {});
            hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
                text(tr("dialogs.settings.general.encoding"), { .expand = false });
                choice(m_encoding, encodingChoices(), { .expand = false })->SetMinSize(wxSize(160, -1));
            });
        });

        separator();

        vbox({ .proportion = 1, .margin = false }, [&] {
            checkBox(m_showLineNumbers, tr("dialogs.settings.general.lineNumbers"));
            checkBox(m_showRightMargin, tr("dialogs.settings.general.rightMargin"));
            checkBox(m_foldMargin, tr("dialogs.settings.general.foldMargin"));
            checkBox(m_changeTracking, tr("dialogs.settings.general.changeTracking"));
            checkBox(m_autoReload, tr("dialogs.settings.general.autoReload"));
            checkBox(m_splashScreen, tr("dialogs.settings.general.splashScreen"));
            checkBox(m_configurationInStatusBar, tr("dialogs.settings.general.configurationInStatusBar"));
            checkBox(m_checkUpdatesOnLoad, tr("dialogs.settings.general.checkUpdatesOnLoad"));
            spinCtrl(m_tabSize, tr("dialogs.settings.general.tabSize"), 1, 16, {});
            hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
                text(tr("dialogs.settings.general.eolMode"), { .expand = false });
                choice(m_eolMode, eolModeChoices(), { .expand = false })->SetMinSize(wxSize(160, -1));
            });
        });
    });

    // FBIde settings: theme (light/dark) and language, laid out in the same
    // two equal-weight columns as the editor settings above so the columns
    // line up. Theme is hidden where wxWidgets can't switch it (e.g. Windows
    // before 10 1903); Language then takes the first column.
    const bool showTheme = isAppearanceSupported();
    const auto themeRow = [&] {
        hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
            text(tr("dialogs.settings.general.appearance"), { .expand = false });
            static constexpr std::array kOptions { wxApp::Appearance::System, wxApp::Appearance::Dark, wxApp::Appearance::Light };
            wxArrayString names;
            names.Add(tr("dialogs.settings.general.appearanceSystem"));
            names.Add(tr("dialogs.settings.general.appearanceDark"));
            names.Add(tr("dialogs.settings.general.appearanceLight"));

            // An unset value shows as System.
            const auto current = parseAppearance(m_appearance).value_or(wxApp::Appearance::System);
            const auto ctrl = choice(names, { .expand = false });
            ctrl->SetMinSize(wxSize(160, -1));
            ctrl->SetSelection(static_cast<int>(std::ranges::find(kOptions, current) - kOptions.begin()));
            ctrl->Bind(wxEVT_CHOICE, [this](const wxCommandEvent& evt) {
                const auto idx = evt.GetSelection();
                if (idx >= 0 && static_cast<std::size_t>(idx) < kOptions.size()) {
                    m_appearance = appearanceConfigKey(kOptions[static_cast<std::size_t>(idx)]);
                }
            });
        });
    };
    const auto languageRow = [&] {
        // The dropdown shows each locale's `name=` header (falling back to
        // the filename) sorted alphabetically; `m_language` keeps storing
        // the filename so the on-disk config shape doesn't change.
        hbox({ .alignment = SmartBoxSizer::Alignment::Center, .margin = false }, [&] {
            text(tr("dialogs.settings.general.language"), { .expand = false });
            const auto languages = loadLanguageOptions(getContext());

            wxArrayString displayNames;
            int selected = wxNOT_FOUND;
            for (std::size_t i = 0; i < languages.size(); i++) {
                displayNames.Add(languages[i].displayName);
                if (languages[i].fileName == m_language) {
                    selected = static_cast<int>(i);
                }
            }

            const auto ctrl = choice(displayNames, { .expand = false });
            ctrl->SetMinSize(wxSize(160, -1));
            if (selected != wxNOT_FOUND) {
                ctrl->SetSelection(selected);
            } else if (!displayNames.IsEmpty()) {
                ctrl->SetSelection(0);
            }
            ctrl->Bind(wxEVT_CHOICE, [this, languages](const wxCommandEvent& evt) {
                const auto idx = evt.GetSelection();
                if (idx >= 0 && static_cast<std::size_t>(idx) < languages.size()) {
                    m_language = languages[static_cast<std::size_t>(idx)].fileName;
                }
            });
        });
    };

    hbox(tr("dialogs.settings.general.ideSettings"), { .margin = false }, [&] {
        vbox({ .proportion = 1, .margin = false }, [&] {
            if (showTheme) {
                themeRow();
            } else {
                languageRow();
            }
        });

        separator();

        vbox({ .proportion = 1, .margin = false }, [&] {
            if (showTheme) {
                languageRow();
            } else {
                // wx treats a sizer with no shown items as hidden and would
                // collapse this column; a spacer keeps it at its share.
                spacer(0);
            }
        });
    });

    SetSizerAndFit(currentSizer());
}

void GeneralPage::apply() {
    auto& cfgManager = getContext().getConfigManager();
    auto& cfg = cfgManager.config();
    auto& editor = cfg["editor"];
    editor["autoIndent"] = m_autoIndent;
    editor["transformKeywords"] = m_transformKeywords;
    editor["indentGuide"] = m_indentGuide;
    editor["whiteSpace"] = m_showWhiteSpaces;
    editor["displayEOL"] = m_showLineEndings;
    editor["braceHighlight"] = m_braceHighlight;
    editor["highlightOccurrences"] = m_highlightOccurrences;
    editor["codeCompletion"] = m_codeCompletion;
    editor["syntaxHighlight"] = m_syntaxHighlight;
    editor["lineNumbers"] = m_showLineNumbers;
    editor["longLine"] = m_showRightMargin;
    editor["folderMargin"] = m_foldMargin;
    editor["changeTracking"] = m_changeTracking;
    editor["autoReload"] = m_autoReload;
    editor["edgeColumn"] = m_edgeColumn;
    editor["tabSize"] = m_tabSize;
    editor["encoding"] = m_encoding;
    editor["eolMode"] = m_eolMode;
    cfg["general"]["splashScreen"] = m_splashScreen;
    cfg["commands"]["configurationInStatusBar"] = m_configurationInStatusBar;
    cfg["update"]["checkOnStartup"] = m_checkUpdatesOnLoad;

    // Appearance is read once at startup (wx can't switch it once windows
    // are shown), so a change is saved now and applies on the next start —
    // the restart offered below just gets there sooner.
    const bool appearanceChanged = m_appearance != cfg.get_or("appearance", "");
    if (appearanceChanged) {
        cfg["appearance"] = m_appearance;
    }

    // Swap locale file if the user picked a different language. Live
    // refresh would have to update every menu/dialog/sidebar string in
    // place — too many small touch points to keep correct. Instead we
    // confirm with the user, snapshot the open documents, and relaunch
    // FBIde to restore them so the new locale loads cleanly. Some editor
    // undo state is lost; the trade-off is worth it
    // for a rarely-changed setting.
    const bool languageChanged = !m_language.empty() && m_language != currentLocaleFileName(cfg);
    if (!languageChanged && !appearanceChanged) {
        return;
    }

    // One prompt even when both changed: the language text covers it.
    const auto answer = wxMessageBox(
        tr(languageChanged ? "dialogs.settings.general.languageRestart" : "dialogs.settings.general.appearanceRestart"),
        tr("dialogs.settings.general.languageRestartTitle"),
        wxYES_NO | wxICON_QUESTION,
        this
    );
    if (answer != wxYES) {
        // Revert the in-memory language selection — the locale stays
        // unchanged. A changed appearance stays saved for the next start.
        m_language = currentLocaleFileName(cfg);
        return;
    }

    // Hand the restart over to App. The locale-path swap is
    // committed inside the callback, which only fires after the
    // session save succeeds — so a cancelled in-flight save
    // leaves the language unchanged.
    std::function<void()> commit;
    if (languageChanged) {
        commit = [&ctx = getContext(), language = m_language]() {
            ctx.getConfigManager().setCategoryPath(
                ConfigManager::Category::Locale,
                "locales/" + language
            );
        };
    }
    getContext().getApp().scheduleRestart(std::move(commit));
}
