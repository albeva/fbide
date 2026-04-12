//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppMemberFunctionMayBeStatic
#include "CommandManager.hpp"
#include "Context.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
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
    EVT_MENU(id(MenuId::ActivePath),    CommandManager::onActivePath)

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
    m_ctx.getDocumentManager().createNew();
}

void CommandManager::onOpen(wxCommandEvent&) {
    m_ctx.getDocumentManager().openWithDialog();
}

void CommandManager::onSave(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().save(*doc);
    }
}

void CommandManager::onSaveAs(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().saveAs(*doc);
    }
}

void CommandManager::onSaveAll(wxCommandEvent&) {
    (void)m_ctx.getDocumentManager().saveAll();
}

void CommandManager::onClose(wxCommandEvent&) {
    if (auto* doc = m_ctx.getDocumentManager().getActive()) {
        m_ctx.getDocumentManager().close(*doc);
    }
}

void CommandManager::onCloseAll(wxCommandEvent&) {
    m_ctx.getDocumentManager().closeAll();
}

void CommandManager::onNewWindow(wxCommandEvent&) {
    // TODO: launch new editor instance
}

void CommandManager::onQuit(wxCommandEvent&) {
    m_ctx.getUIManager().getMainFrame()->Close();
}

void CommandManager::onSessionLoad(wxCommandEvent&) {
    // TODO: implement session load
}

void CommandManager::onSessionSave(wxCommandEvent&) {
    // TODO: implement session save
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
    // TODO: implement comment block
}

void CommandManager::onUncomment(wxCommandEvent&) {
    // TODO: implement uncomment block
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
    // TODO: implement format dialog
}

void CommandManager::onResult(wxCommandEvent&) {
    // TODO: toggle console pane
}

void CommandManager::onSubs(wxCommandEvent&) {
    // TODO: implement sub/function browser
}

void CommandManager::onCompilerLog(wxCommandEvent&) {
    // TODO: implement compiler log
}

// -- Run --

void CommandManager::onCompile(wxCommandEvent&) {
    // TODO: implement compile
}

void CommandManager::onCompileAndRun(wxCommandEvent&) {
    // TODO: implement compile and run
}

void CommandManager::onRun(wxCommandEvent&) {
    // TODO: implement run
}

void CommandManager::onQuickRun(wxCommandEvent&) {
    // TODO: implement quick run
}

void CommandManager::onCmdPrompt(wxCommandEvent&) {
    // TODO: open command prompt
}

void CommandManager::onParameters(wxCommandEvent&) {
    // TODO: implement parameters dialog
}

void CommandManager::onShowExitCode(wxCommandEvent&) {
    // TODO: toggle show exit code
}

void CommandManager::onActivePath(wxCommandEvent&) {
    // TODO: toggle active path
}

// -- Help --

void CommandManager::onHelp(wxCommandEvent&) {
    // TODO: open help file
}

void CommandManager::onQuickKeys(wxCommandEvent&) {
    // TODO: open quickkeys.txt
}

void CommandManager::onReadMe(wxCommandEvent&) {
    // TODO: open readme.txt
}

void CommandManager::onAbout(wxCommandEvent&) {
    // TODO: implement about dialog
}
