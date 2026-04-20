//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppMemberFunctionMayBeStatic
#include "CommandManager.hpp"

#include <ranges>

#include "about/AboutDialog.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/Document.hpp"
#include "editor/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "format/FormatDialog.hpp"
#include "help/HelpManager.hpp"
#include "settings/SettingsDialog.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(CommandManager, wxEvtHandler)
    EVT_MENU(wxID_ANY, CommandManager::onAnyEvent)
    // File
    EVT_MENU(+CommandId::New,          CommandManager::onNew)
    EVT_MENU(+CommandId::Open,         CommandManager::onOpen)
    EVT_MENU(+CommandId::Save,         CommandManager::onSave)
    EVT_MENU(+CommandId::SaveAs,       CommandManager::onSaveAs)
    EVT_MENU(+CommandId::SaveAll,      CommandManager::onSaveAll)
    EVT_MENU(+CommandId::Close,        CommandManager::onClose)
    EVT_MENU(+CommandId::CloseAll,     CommandManager::onCloseAll)
    EVT_MENU(+CommandId::NewWindow,    CommandManager::onNewWindow)
    EVT_MENU(+CommandId::Quit,         CommandManager::onQuit)
    EVT_MENU(+CommandId::SessionLoad,  CommandManager::onSessionLoad)
    EVT_MENU(+CommandId::SessionSave,  CommandManager::onSessionSave)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, CommandManager::onFileHistory)

    // Edit
    EVT_MENU(+CommandId::Undo,           CommandManager::onUndo)
    EVT_MENU(+CommandId::Redo,           CommandManager::onRedo)
    EVT_MENU(+CommandId::Cut,            CommandManager::onCut)
    EVT_MENU(+CommandId::Copy,           CommandManager::onCopy)
    EVT_MENU(+CommandId::Paste,          CommandManager::onPaste)
    EVT_MENU(+CommandId::SelectAll,      CommandManager::onSelectAll)
    EVT_MENU(+CommandId::SelectLine,     CommandManager::onSelectLine)
    EVT_MENU(+CommandId::IndentIncrease, CommandManager::onIndentIncrease)
    EVT_MENU(+CommandId::IndentDecrease, CommandManager::onIndentDecrease)
    EVT_MENU(+CommandId::Comment,        CommandManager::onComment)
    EVT_MENU(+CommandId::Uncomment,      CommandManager::onUncomment)

    // Search
    EVT_MENU(+CommandId::Find,      CommandManager::onFind)
    EVT_MENU(+CommandId::FindNext,  CommandManager::onFindNext)
    EVT_MENU(+CommandId::Replace,   CommandManager::onReplace)
    EVT_MENU(+CommandId::GotoLine,  CommandManager::onGotoLine)

    // View
    EVT_MENU(+CommandId::Preferences, CommandManager::onSettings)
    EVT_MENU(+CommandId::Format,      CommandManager::onFormat)
    // EVT_MENU(+CommandId::Result,      CommandManager::onResult)
    EVT_MENU(+CommandId::Subs,        CommandManager::onSubs)
    EVT_MENU(+CommandId::CompilerLog, CommandManager::onCompilerLog)

    // Run
    EVT_MENU(+CommandId::Compile,       CommandManager::onCompile)
    EVT_MENU(+CommandId::CompileAndRun, CommandManager::onCompileAndRun)
    EVT_MENU(+CommandId::Run,           CommandManager::onRun)
    EVT_MENU(+CommandId::QuickRun,      CommandManager::onQuickRun)
    EVT_MENU(+CommandId::KillProcess,   CommandManager::onKillProcess)
    EVT_MENU(+CommandId::CmdPrompt,     CommandManager::onCmdPrompt)
    EVT_MENU(+CommandId::Parameters,    CommandManager::onParameters)

    // Help
    EVT_MENU(+CommandId::Help,      CommandManager::onHelp)
    EVT_MENU(+CommandId::About,     CommandManager::onAbout)
wxEND_EVENT_TABLE()
// clang-format on

CommandManager::CommandManager(Context& ctx)
: m_ctx(ctx) {
    // clang-format off
    addCommands({
        // Main menu entries
        CommandEntry { .name="menus.file",   .kind = wxITEM_DROPDOWN },
        CommandEntry { .name="menus.edit",   .kind = wxITEM_DROPDOWN },
        CommandEntry { .name="menus.search", .kind = wxITEM_DROPDOWN },
        CommandEntry { .name="menus.view",   .kind = wxITEM_DROPDOWN },
        CommandEntry { .name="menus.run",    .kind = wxITEM_DROPDOWN },
        CommandEntry { .name="menus.help",   .kind = wxITEM_DROPDOWN },
        // commands
        CommandEntry { .id = +CommandId::About,            .name="about" },
        CommandEntry { .id = +CommandId::Close,            .name="close" },
        CommandEntry { .id = +CommandId::CloseAll,         .name="closeAll" },
        CommandEntry { .id = +CommandId::CmdPrompt,        .name="cmdPrompt" },
        CommandEntry { .id = +CommandId::Comment,          .name="comment" },
        CommandEntry { .id = +CommandId::Compile,          .name="compile" },
        CommandEntry { .id = +CommandId::CompileAndRun,    .name="compileAndRun" },
        CommandEntry { .id = +CommandId::CompilerLog,      .name="compilerLog" },
        CommandEntry { .id = +CommandId::Copy,             .name="copy" },
        CommandEntry { .id = +CommandId::Cut,              .name="cut" },
        CommandEntry { .id = +CommandId::FileHistory,      .name="fileHistory" },
        CommandEntry { .id = +CommandId::Find,             .name="find" },
        CommandEntry { .id = +CommandId::FindNext,         .name="findNext" },
        CommandEntry { .id = +CommandId::FindPrevious,     .name="findPrevious" },
        CommandEntry { .id = +CommandId::Format,           .name="format" },
        CommandEntry { .id = +CommandId::GotoLine,         .name="gotoLine" },
        CommandEntry { .id = +CommandId::Help,             .name="help"  },
        CommandEntry { .id = +CommandId::IndentDecrease,   .name="indentDec" },
        CommandEntry { .id = +CommandId::IndentIncrease,   .name="indentInc" },
        CommandEntry { .id = +CommandId::New,              .name="new" },
        CommandEntry { .id = +CommandId::NewWindow,        .name="newWindow" },
        CommandEntry { .id = +CommandId::Open,             .name="open" },
        CommandEntry { .id = +CommandId::Parameters,       .name="parameters" },
        CommandEntry { .id = +CommandId::Paste,            .name="paste" },
        CommandEntry { .id = +CommandId::Preferences,      .name="settings" },
        CommandEntry { .id = +CommandId::QuickRun,         .name="quickRun" },
        CommandEntry { .id = +CommandId::KillProcess,      .name="killProcess" },
        CommandEntry { .id = +CommandId::Quit,             .name="quit"  },
        CommandEntry { .id = +CommandId::RecentFiles,      .name="recentFiles", .kind = wxITEM_DROPDOWN },
        CommandEntry { .id = +CommandId::ClearRecentFiles, .name="clearRecentFiles" },
        CommandEntry { .id = +CommandId::Redo,             .name="redo" },
        CommandEntry { .id = +CommandId::Replace,          .name="replace" },
        CommandEntry { .id = +CommandId::Result,           .name="viewResult", .kind = wxITEM_CHECK },
        CommandEntry { .id = +CommandId::Run,              .name="run" },
        CommandEntry { .id = +CommandId::Save,             .name="save" },
        CommandEntry { .id = +CommandId::SaveAll,          .name="saveAll" },
        CommandEntry { .id = +CommandId::SaveAs,           .name="saveAs" },
        CommandEntry { .id = +CommandId::SelectAll,        .name="selectAll" },
        CommandEntry { .id = +CommandId::SelectLine,       .name="selectLine" },
        CommandEntry { .id = +CommandId::SessionLoad,      .name="sessionLoad" },
        CommandEntry { .id = +CommandId::SessionSave,      .name="sessionSave" },
        CommandEntry { .id = +CommandId::ShowExitCode,     .name="showExitCode", .kind = wxITEM_CHECK },
        CommandEntry { .id = +CommandId::Subs,             .name="viewSubs" },
        CommandEntry { .id = +CommandId::Uncomment,        .name="uncomment" },
        CommandEntry { .id = +CommandId::Undo,             .name="undo" },
    });
    // clang-format on
}

void CommandManager::initializeCommands(){
    auto& commands = m_ctx.getConfigManager().config()["commands"];
    for (auto& entry : m_namedCommands | std::views::values) {
        if (entry.kind == wxITEM_CHECK) {
            auto& node = commands[entry.name];
            if (const auto existing = node.as<bool>()) {
                entry.checked = *existing;
            } else {
                node = entry.checked;
            }
            entry.update();
        }
    }
}

void CommandManager::onAnyEvent(wxCommandEvent& event) {
    event.Skip();
    const auto thaw = m_ctx.getUIManager().freeze();

    if (auto* entry = find(event.GetId())) {
        if (entry->kind != wxITEM_CHECK || entry->checked == event.IsChecked()) {
            return;
        }
        m_ctx.getConfigManager().config()["commands"][entry->name] = event.IsChecked();
        entry->setChecked(event.IsChecked());
    }
}

// region ---------- Event Handling ----------

void CommandManager::onNew(wxCommandEvent&) {
    m_ctx.getDocumentManager().newFile();
}

void CommandManager::onOpen(wxCommandEvent&) {
    m_ctx.getDocumentManager().openFile();
}

void CommandManager::onSave(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().saveFile(*doc);
    }
}

void CommandManager::onSaveAs(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().saveFileAs(*doc);
    }
}

void CommandManager::onSaveAll(wxCommandEvent&) {
    (void)m_ctx.getDocumentManager().saveAllFiles();
}

void CommandManager::onClose(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().closeFile(*doc);
    }
}

void CommandManager::onCloseAll(wxCommandEvent&) {
    m_ctx.getDocumentManager().closeAllFiles();
}

void CommandManager::onNewWindow(wxCommandEvent&) {
    const auto exe = wxStandardPaths::Get().GetExecutablePath();
    wxExecute("\"" + exe + "\" --new-window");
}

void CommandManager::onQuit(wxCommandEvent&) {
    m_ctx.getUIManager().getMainFrame()->Close();
}

void CommandManager::onSessionLoad(wxCommandEvent&) {
    m_ctx.getDocumentManager().loadSession();
}

void CommandManager::onSessionSave(wxCommandEvent&) {
    m_ctx.getDocumentManager().saveSession();
}

void CommandManager::onFileHistory(wxCommandEvent& event) {
    const auto idx = static_cast<size_t>(event.GetId() - wxID_FILE1);
    if (const auto file = m_ctx.getFileHistory().getFile(idx)) {
        m_ctx.getDocumentManager().openFile(*file);
    }
}

void CommandManager::onUndo(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->Undo();
    }
}

void CommandManager::onRedo(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->Redo();
    }
}

void CommandManager::onCut(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->Cut();
    }
}

void CommandManager::onCopy(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->Copy();
    }
}

void CommandManager::onPaste(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->Paste();
    }
}

void CommandManager::onSelectAll(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->SelectAll();
    }
}

void CommandManager::onSelectLine(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->selectLine();
    }
}

void CommandManager::onIndentIncrease(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->CmdKeyExecute(wxSTC_CMD_TAB);
    }
}

void CommandManager::onIndentDecrease(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->CmdKeyExecute(wxSTC_CMD_BACKTAB);
    }
}

void CommandManager::onComment(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->commentSelection();
    }
}

void CommandManager::onUncomment(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        doc->getEditor()->uncommentSelection();
    }
}

void CommandManager::onFind(wxCommandEvent&) {
    m_ctx.getDocumentManager().showFind();
}

void CommandManager::onFindNext(wxCommandEvent&) {
    m_ctx.getDocumentManager().findNext();
}

void CommandManager::onReplace(wxCommandEvent&) {
    m_ctx.getDocumentManager().showReplace();
}

void CommandManager::onGotoLine(wxCommandEvent&) {
    m_ctx.getDocumentManager().gotoLine();
}

void CommandManager::onSettings(wxCommandEvent&) {
    SettingsDialog dlg(m_ctx.getUIManager().getMainFrame(), m_ctx);
    dlg.create();
    dlg.ShowModal();
}

void CommandManager::onFormat(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive(); doc->getEditor() != nullptr) {
        FormatDialog dlg(m_ctx.getUIManager().getMainFrame(), m_ctx, doc);
        dlg.create();
        dlg.ShowModal();
    }
}

void CommandManager::onSubs(wxCommandEvent&) {
    // TODO: implement sub/function browser
}

void CommandManager::onCompilerLog(wxCommandEvent&) {
    m_ctx.getCompilerManager().showCompilerLog();
}

void CommandManager::onCompile(wxCommandEvent&) {
    m_ctx.getCompilerManager().compile();
}

void CommandManager::onCompileAndRun(wxCommandEvent&) {
    m_ctx.getCompilerManager().compileAndRun();
}

void CommandManager::onRun(wxCommandEvent&) {
    m_ctx.getCompilerManager().run();
}

void CommandManager::onQuickRun(wxCommandEvent&) {
    m_ctx.getCompilerManager().quickRun();
}

void CommandManager::onKillProcess(wxCommandEvent&) {
    m_ctx.getCompilerManager().killProcess();
}

void CommandManager::onCmdPrompt(wxCommandEvent&) {
    // Working directory: active document's folder or IDE folder
    wxString cwd;
    if (const auto* doc = m_ctx.getDocumentManager().getActive(); doc != nullptr && !doc->isNew()) {
        cwd = wxPathOnly(doc->getFilePath());
    } else {
        cwd = m_ctx.getConfigManager().getAppDir();
    }

    wxSetWorkingDirectory(cwd);
    wxExecute(ConfigManager::getTerminal());
}

void CommandManager::onParameters(wxCommandEvent&) {
    wxTextEntryDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("dialogs.runParams.prompt"),
        m_ctx.tr("dialogs.settings.themes.nameDialogTitle"),
        m_parameters,
        wxOK | wxCANCEL
    );
    if (dlg.ShowModal() == wxID_OK) {
        m_parameters = dlg.GetValue();
    }
}

void CommandManager::onHelp(wxCommandEvent&) {
    m_ctx.getHelpManager().open();
}

void CommandManager::onAbout(wxCommandEvent&) {
    AboutDialog dlg(m_ctx.getUIManager().getMainFrame(), m_ctx);
    dlg.create();
    dlg.ShowModal();
}

// endregion

// region ---------- Command Handling ----------

void CommandManager::addCommands(const std::initializer_list<CommandEntry>& commands) {
    const auto size = m_namedCommands.size() + commands.size();
    m_namedCommands.reserve(size);
    m_namedCommands.reserve(size);
    for (const auto& cmd : commands) {
        const auto id = cmd.id == 0 || cmd.id == wxID_ANY ? wxNewId() : cmd.id;
        auto& entry = m_namedCommands[cmd.name] = cmd;
        entry.id = id;
        m_idNames[id] = cmd.name;
    }
}

auto CommandManager::find(const wxString& name)-> CommandEntry* {
    const auto iter = m_namedCommands.find(name);
    if (iter != m_namedCommands.end()) {
        return &iter->second;
    }
    return nullptr;
}

auto CommandManager::find(const wxString& name) const-> const CommandEntry* {
    const auto iter = m_namedCommands.find(name);
    if (iter != m_namedCommands.end()) {
        return &iter->second;
    }
    return nullptr;
}

auto CommandManager::find(const wxWindowID id)-> CommandEntry* {
    const auto iter = m_idNames.find(id);
    if (iter != m_idNames.end()) {
        return find(iter->second);
    }
    return nullptr;
}

auto CommandManager::find(const wxWindowID id) const-> const CommandEntry* {
    const auto iter = m_idNames.find(id);
    if (iter != m_idNames.end()) {
        return find(iter->second);
    }
    return nullptr;
}

// endregion
