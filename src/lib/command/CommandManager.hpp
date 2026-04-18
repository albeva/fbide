//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "CommandEntry.hpp"
#include "CommandId.hpp"

namespace fbide {
class Context;

/// Routes menu and toolbar commands to the appropriate managers.
/// Thin handler — no logic, just dispatch.
class CommandManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(CommandManager)

    explicit CommandManager(Context& ctx);

    /// Add managed command entries
    void addCommands(const std::initializer_list<CommandEntry>& commands);

    /// get command by name, return nullptr if not found
    [[nodiscard]] auto find(const wxString& name) -> CommandEntry*;
    [[nodiscard]] auto find(const wxString& name) const -> const CommandEntry*;

    /// Found bound control for given entry, return nullptr if nothing found
    template<typename T>
    [[nodiscard]] auto find(const wxString& name) const -> T* {
        if (const auto* found = find(name)) {
            return found->get<T>();
        }
        return nullptr;
    }

    /// get command by id, return nullptr if not found
    [[nodiscard]] auto find(wxWindowID id) -> CommandEntry*;
    [[nodiscard]] auto find(wxWindowID id) const -> const CommandEntry*;

    /// Found bound control for given entry, return nullptr if nothing found
    template<typename T>
    [[nodiscard]] auto find(const wxWindowID id) const -> T* {
        if (const auto* found = find(id)) {
            return found->get<T>();
        }
        return nullptr;
    }

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

    // Help
    void onHelp(wxCommandEvent& event);
    void onQuickKeys(wxCommandEvent& event);
    void onReadMe(wxCommandEvent& event);
    void onAbout(wxCommandEvent& event);

    Context& m_ctx;
    wxString m_parameters;
    std::unordered_map<wxString, CommandEntry> m_namedCommands;
    std::unordered_map<wxWindowID, wxString> m_idNames;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
