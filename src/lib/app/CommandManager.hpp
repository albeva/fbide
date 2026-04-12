//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Routes menu and toolbar commands to the appropriate managers.
/// Thin handler — no logic, just dispatch.
class CommandManager final : public wxEvtHandler {
public:
    explicit CommandManager(Context& ctx);

private:
    // File
    void onNew(wxCommandEvent& event);
    void onOpen(wxCommandEvent& event);
    void onSave(wxCommandEvent& event);
    void onSaveAs(wxCommandEvent& event);
    void onSaveAll(wxCommandEvent& event);
    void onClose(wxCommandEvent& event);
    void onCloseAll(wxCommandEvent& event);
    void onNewWindow(wxCommandEvent& event);
    void onQuit(wxCommandEvent& event);
    void onSessionLoad(wxCommandEvent& event);
    void onSessionSave(wxCommandEvent& event);
    void onFileHistory(wxCommandEvent& event);

    // Edit
    void onUndo(wxCommandEvent& event);
    void onRedo(wxCommandEvent& event);
    void onCut(wxCommandEvent& event);
    void onCopy(wxCommandEvent& event);
    void onPaste(wxCommandEvent& event);
    void onSelectAll(wxCommandEvent& event);
    void onSelectLine(wxCommandEvent& event);
    void onIndentIncrease(wxCommandEvent& event);
    void onIndentDecrease(wxCommandEvent& event);
    void onComment(wxCommandEvent& event);
    void onUncomment(wxCommandEvent& event);

    // Search
    void onFind(wxCommandEvent& event);
    void onFindNext(wxCommandEvent& event);
    void onReplace(wxCommandEvent& event);
    void onGotoLine(wxCommandEvent& event);

    // View
    void onSettings(wxCommandEvent& event);
    void onFormat(wxCommandEvent& event);
    void onResult(wxCommandEvent& event);
    void onSubs(wxCommandEvent& event);
    void onCompilerLog(wxCommandEvent& event);

    // Run
    void onCompile(wxCommandEvent& event);
    void onCompileAndRun(wxCommandEvent& event);
    void onRun(wxCommandEvent& event);
    void onQuickRun(wxCommandEvent& event);
    void onCmdPrompt(wxCommandEvent& event);
    void onParameters(wxCommandEvent& event);
    void onShowExitCode(wxCommandEvent& event);
    void onActivePath(wxCommandEvent& event);

    // Help
    void onHelp(wxCommandEvent& event);
    void onQuickKeys(wxCommandEvent& event);
    void onReadMe(wxCommandEvent& event);
    void onAbout(wxCommandEvent& event);

    Context& m_ctx;
    wxString m_parameters;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
