//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "GeneralPage.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/FileSession.hpp"
#include "document/TextEncoding.hpp"
#include "ui/UIManager.hpp"
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

} // namespace

GeneralPage::GeneralPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    auto& cfg = getContext().getConfigManager().config();
    const auto& editor = cfg.at("editor");
    m_autoIndent = editor.get_or("autoIndent", true);
    m_transformKeywords = editor.get_or("transformKeywords", true);
    m_indentGuide = editor.get_or("indentGuide", false);
    m_showWhiteSpaces = editor.get_or("whiteSpace", false);
    m_showLineEndings = editor.get_or("displayEOL", false);
    m_braceHighlight = editor.get_or("braceHighlight", true);
    m_syntaxHighlight = editor.get_or("syntaxHighlight", true);
    m_showLineNumbers = editor.get_or("lineNumbers", true);
    m_showRightMargin = editor.get_or("longLine", false);
    m_foldMargin = editor.get_or("folderMargin", false);
    m_edgeColumn = editor.get_or("edgeColumn", 80);
    m_tabSize = editor.get_or("tabSize", 4);
    m_encoding = editor.get_or("encoding", "UTF-8");
    m_eolMode = editor.get_or("eolMode", "LF");
    m_splashScreen = cfg.get_or("general.splashScreen", true);
    m_language = currentLocaleFileName(cfg);
}

void GeneralPage::create() {
    hbox(tr("dialogs.settings.general.editorSettings"), { .border = 0 }, [&] {
        vbox({ .proportion = 1 }, [&] {
            checkBox(m_autoIndent, tr("dialogs.settings.general.autoIndent"));
            checkBox(m_transformKeywords, tr("dialogs.settings.general.transformKeywords"));
            checkBox(m_indentGuide, tr("dialogs.settings.general.indentGuides"));
            checkBox(m_showWhiteSpaces, tr("dialogs.settings.general.whitespace"));
            checkBox(m_showLineEndings, tr("dialogs.settings.general.lineEndings"));
            checkBox(m_braceHighlight, tr("dialogs.settings.general.braceHighlight"));
            spinCtrl(m_edgeColumn, tr("dialogs.settings.general.rightMarginWidth"), 1, 200, {});
            hbox({ .center = true, .border = 0 }, [&] {
                text(tr("dialogs.settings.general.encoding"), { .expand = false });
                choice(m_encoding, encodingChoices(), { .expand = false })->SetMinSize(wxSize(160, -1));
            });
        });

        separator({ .space = false });

        vbox({ .proportion = 1 }, [&] {
            checkBox(m_syntaxHighlight, tr("dialogs.settings.general.syntaxHighlight"));
            checkBox(m_showLineNumbers, tr("dialogs.settings.general.lineNumbers"));
            checkBox(m_showRightMargin, tr("dialogs.settings.general.rightMargin"));
            checkBox(m_foldMargin, tr("dialogs.settings.general.foldMargin"));
            checkBox(m_splashScreen, tr("dialogs.settings.general.splashScreen"));
            spinCtrl(m_tabSize, tr("dialogs.settings.general.tabSize"), 1, 16, {});
            hbox({ .center = true, .border = 0 }, [&] {
                text(tr("dialogs.settings.general.eolMode"), { .expand = false });
                choice(m_eolMode, eolModeChoices(), { .expand = false })->SetMinSize(wxSize(160, -1));
            });
        });
    });

    // Language section
    vbox(tr("dialogs.settings.general.language"), {}, [&] {
        hbox({ .center = true, .border = 0 }, [&] {
            text(tr("dialogs.settings.general.languageSelect"), { .proportion = 1, .expand = false });
            wxArrayString names;
            for (const auto& path : getContext().getConfigManager().getAllLanguages()) {
                names.Add(wxFileName(path).GetFullName());
            }
            choice(m_language, names, { .expand = false })->SetMinSize(wxSize(200, -1));
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
    editor["syntaxHighlight"] = m_syntaxHighlight;
    editor["lineNumbers"] = m_showLineNumbers;
    editor["longLine"] = m_showRightMargin;
    editor["folderMargin"] = m_foldMargin;
    editor["edgeColumn"] = m_edgeColumn;
    editor["tabSize"] = m_tabSize;
    editor["encoding"] = m_encoding;
    editor["eolMode"] = m_eolMode;
    cfg["general"]["splashScreen"] = m_splashScreen;

    // Swap locale file if the user picked a different language. Live
    // refresh would have to update every menu/dialog/sidebar string in
    // place — too many small touch points to keep correct. Instead we
    // confirm with the user, save the open documents to a temp session,
    // and relaunch FBIde with `--load-session` so the new locale loads
    // cleanly. Some editor undo state is lost; the trade-off is worth it
    // for a rarely-changed setting.
    if (!m_language.empty() && m_language != currentLocaleFileName(cfg)) {
        const auto answer = wxMessageBox(
            tr("dialogs.settings.general.languageRestart"),
            tr("dialogs.settings.general.languageRestartTitle"),
            wxYES_NO | wxICON_QUESTION,
            this
        );
        if (answer != wxYES) {
            // Revert the in-memory selection — config stays unchanged.
            m_language = currentLocaleFileName(cfg);
            return;
        }

        // Defer the restart until after the Settings dialog has closed
        // and `SettingsDialog::applyChanges` has saved the config. We
        // intentionally do NOT swap the locale path here — that happens
        // inside the lambda, only after we know the session save went
        // through. If the user cancels a "save changes?" dialog mid-way,
        // we abort the restart and the language stays as it was.
        wxTheApp->CallAfter([&ctx = getContext(), language = m_language]() {
            auto& uiMgr = ctx.getUIManager();
            auto& cfgMgr = ctx.getConfigManager();

            // Persist the open documents to a temp session. If any
            // in-flight save dialog is cancelled, FileSession returns
            // false — abort the restart entirely.
            const auto sessionPath = cfgMgr.getIdeDir() / "restart-session.fbs";
            if (!ctx.getFileSession().save(sessionPath)) {
                return;
            }

            // Commit the locale swap and persist state.
            cfgMgr.setCategoryPath(ConfigManager::Category::Locale, "locales/" + language);
            uiMgr.saveWindowGeometry();
            cfgMgr.save(ConfigManager::Category::Config);
            ctx.getFileHistory().save();

            // Spawn the replacement through a shell wrapper that waits
            // briefly before launching FBIde. The delay gives this
            // process time to fully exit so the new window doesn't
            // appear alongside the old one.
            const auto exe = wxStandardPaths::Get().GetExecutablePath();
#ifdef __WXMSW__
            // `ping -n 2 127.0.0.1` waits ~1s and works on every
            // Windows from XP up (Vista's `timeout` doesn't).
            // `start "" ...` detaches the new fbide from cmd.
            const auto cmd = wxString::Format(
                R"(cmd.exe /c "ping -n 2 127.0.0.1 >nul & start "" "%s" --new-window --load-session "%s"")",
                exe, sessionPath
            );
#else
            const auto cmd = wxString::Format(
                R"(sh -c 'sleep 1 && "%s" --new-window --load-session "%s" &')",
                exe, sessionPath
            );
#endif
            wxExecute(cmd);

            // Trigger the normal close path. Mark documents
            // not-modified so `prepareToQuit` doesn't re-prompt
            // (FileSession already covered them); `UIManager::onClose`
            // handles window geometry / config / history persistence
            // again on its way out, which is fine — the writes are
            // idempotent.
            for (const auto& doc : ctx.getDocumentManager().getDocuments()) {
                doc->setModified(false);
            }
            ctx.getUIManager().getMainFrame()->Close(true);
        });
    }
}
