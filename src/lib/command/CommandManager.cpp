//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppMemberFunctionMayBeStatic
#include "CommandManager.hpp"
#include "lib/about/AboutDialog.hpp"
#include "lib/app/Context.hpp"
#include "lib/compiler/CompilerManager.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/FileHistory.hpp"
#include "lib/config/Lang.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/format/FormatDialog.hpp"
#include "lib/help/HelpManager.hpp"
#include "lib/settings/SettingsDialog.hpp"
#include "lib/ui/MenuId.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr auto id(MenuId mid) -> int { return static_cast<int>(mid); }
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(CommandManager, wxEvtHandler)
    // File
    EVT_MENU(id(MenuId::New),          CommandManager::onNew)
    EVT_MENU(id(MenuId::Open),         CommandManager::onOpen)
    EVT_MENU(id(MenuId::Save),         CommandManager::onSave)
    EVT_MENU(id(MenuId::SaveAs),       CommandManager::onSaveAs)
    EVT_MENU(id(MenuId::SaveAll),      CommandManager::onSaveAll)
    EVT_MENU(id(MenuId::Close),        CommandManager::onClose)
    EVT_MENU(id(MenuId::CloseAll),     CommandManager::onCloseAll)
    EVT_MENU(id(MenuId::NewWindow),    CommandManager::onNewWindow)
    EVT_MENU(id(MenuId::Quit),         CommandManager::onQuit)
    EVT_MENU(id(MenuId::SessionLoad),  CommandManager::onSessionLoad)
    EVT_MENU(id(MenuId::SessionSave),  CommandManager::onSessionSave)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, CommandManager::onFileHistory)

    // Edit
    EVT_MENU(id(MenuId::Undo),           CommandManager::onUndo)
    EVT_MENU(id(MenuId::Redo),           CommandManager::onRedo)
    EVT_MENU(id(MenuId::Cut),            CommandManager::onCut)
    EVT_MENU(id(MenuId::Copy),           CommandManager::onCopy)
    EVT_MENU(id(MenuId::Paste),          CommandManager::onPaste)
    EVT_MENU(id(MenuId::SelectAll),      CommandManager::onSelectAll)
    EVT_MENU(id(MenuId::SelectLine),     CommandManager::onSelectLine)
    EVT_MENU(id(MenuId::IndentIncrease), CommandManager::onIndentIncrease)
    EVT_MENU(id(MenuId::IndentDecrease), CommandManager::onIndentDecrease)
    EVT_MENU(id(MenuId::Comment),        CommandManager::onComment)
    EVT_MENU(id(MenuId::Uncomment),      CommandManager::onUncomment)

    // Search
    EVT_MENU(id(MenuId::Find),      CommandManager::onFind)
    EVT_MENU(id(MenuId::FindNext),  CommandManager::onFindNext)
    EVT_MENU(id(MenuId::Replace),   CommandManager::onReplace)
    EVT_MENU(id(MenuId::GotoLine),  CommandManager::onGotoLine)

    // View
    EVT_MENU(id(MenuId::Settings),    CommandManager::onSettings)
    EVT_MENU(id(MenuId::Format),      CommandManager::onFormat)
    EVT_MENU(id(MenuId::Result),      CommandManager::onResult)
    EVT_MENU(id(MenuId::Subs),        CommandManager::onSubs)
    EVT_MENU(id(MenuId::CompilerLog), CommandManager::onCompilerLog)

    // Run
    EVT_MENU(id(MenuId::Compile),       CommandManager::onCompile)
    EVT_MENU(id(MenuId::CompileAndRun), CommandManager::onCompileAndRun)
    EVT_MENU(id(MenuId::Run),           CommandManager::onRun)
    EVT_MENU(id(MenuId::QuickRun),      CommandManager::onQuickRun)
    EVT_MENU(id(MenuId::CmdPrompt),     CommandManager::onCmdPrompt)
    EVT_MENU(id(MenuId::Parameters),    CommandManager::onParameters)
    EVT_MENU(id(MenuId::ShowExitCode),  CommandManager::onShowExitCode)

    // Help
    EVT_MENU(id(MenuId::Help),      CommandManager::onHelp)
    EVT_MENU(id(MenuId::QuickKeys), CommandManager::onQuickKeys)
    EVT_MENU(id(MenuId::ReadMe),    CommandManager::onReadMe)
    EVT_MENU(id(MenuId::About),     CommandManager::onAbout)
wxEND_EVENT_TABLE()
// clang-format on

CommandManager::CommandManager(Context& ctx)
: m_ctx(ctx) {}

// -- File --

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

// -- Edit --

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

// -- Search --

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

// -- View --

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

void CommandManager::onResult(wxCommandEvent&) {
    m_ctx.getUIManager().toggleConsole();
}

void CommandManager::onSubs(wxCommandEvent&) {
    // TODO: implement sub/function browser
}

void CommandManager::onCompilerLog(wxCommandEvent&) {
    m_ctx.getCompilerManager().showCompilerLog();
}

// -- Run --

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

void CommandManager::onCmdPrompt(wxCommandEvent&) {
    // Working directory: active document's folder or IDE folder
    wxString cwd;
    if (const auto* doc = m_ctx.getDocumentManager().getActive(); doc != nullptr && !doc->isNew()) {
        cwd = wxPathOnly(doc->getFilePath());
    } else {
        cwd = m_ctx.getConfig().getAppPath();
    }

    wxSetWorkingDirectory(cwd);
    wxExecute(Config::getTerminal());
}

void CommandManager::onParameters(wxCommandEvent&) {
    const auto& lang = m_ctx.getLang();
    wxTextEntryDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::RunParamsPrompt],
        lang[LangId::ThemeParametersTitle],
        m_parameters,
        wxOK | wxCANCEL
    );
    if (dlg.ShowModal() == wxID_OK) {
        m_parameters = dlg.GetValue();
    }
}

void CommandManager::onShowExitCode(wxCommandEvent&) {
    auto& config = m_ctx.getConfig();
    config.setShowExitCode(!config.getShowExitCode());
}

// -- Help --

void CommandManager::onHelp(wxCommandEvent&) {
    m_ctx.getHelpManager().open();
}

void CommandManager::onQuickKeys(wxCommandEvent&) {
    const auto path = m_ctx.getConfig().getAppSettingsPath() + "quickkeys.txt";
    m_ctx.getDocumentManager().openFile(path);
}

void CommandManager::onReadMe(wxCommandEvent&) {
    const auto path = m_ctx.getConfig().getAppSettingsPath() + "readme.txt";
    m_ctx.getDocumentManager().openFile(path);
}

void CommandManager::onAbout(wxCommandEvent&) {
    AboutDialog dlg(m_ctx.getUIManager().getMainFrame(), m_ctx);
    dlg.create();
    dlg.ShowModal();
}
