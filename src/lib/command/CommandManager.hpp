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

/**
 * Owns the application-wide command table and routes `wxEVT_MENU`
 * events to per-command handlers. Each `CommandEntry` carries a
 * stable internal name, a `CommandId` enum value, and zero or more
 * UI bindings (menu item, toolbar tool, AUI pane, config toggle).
 *
 * **Owns:** `m_namedCommands` (string-keyed) + `m_idNames`
 * (id-keyed) + the dynamic external-link id mapping.
 * **Owned by:** `Context` — declared *last* so handlers can call
 * into any other manager.
 * **Threading:** UI thread only.
 * **Logic:** intentionally thin — every handler dispatches into
 * the appropriate manager via `m_ctx`.
 *
 * See @ref commands.
 */
class CommandManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(CommandManager)

    /// Construct, seeding the command table with every static entry.
    explicit CommandManager(Context& ctx);

    /// Bulk-insert command entries into the lookup tables.
    void addCommands(const std::initializer_list<CommandEntry>& commands);

    /// Lookup by internal name. Returns `nullptr` when missing.
    [[nodiscard]] auto find(const wxString& name) -> CommandEntry*;
    /// Const overload of `find(name)`.
    [[nodiscard]] auto find(const wxString& name) const -> const CommandEntry*;

    /// Lookup the bound control of type `T` for the named entry.
    /// Returns `nullptr` if the entry is missing or has no `T*` bind.
    template<typename T>
    [[nodiscard]] auto find(const wxString& name) const -> T* {
        if (const auto* found = find(name)) {
            return found->get<T>();
        }
        return nullptr;
    }

    /// Lookup by `wxWindowID`. Returns `nullptr` when missing.
    [[nodiscard]] auto find(wxWindowID id) -> CommandEntry*;
    /// Const overload of `find(id)`.
    [[nodiscard]] auto find(wxWindowID id) const -> const CommandEntry*;

    /// Lookup the bound control of type `T` for the entry with `id`.
    /// Returns `nullptr` if the entry is missing or has no `T*` bind.
    template<typename T>
    [[nodiscard]] auto find(const wxWindowID id) const -> T* {
        if (const auto* found = find(id)) {
            return found->get<T>();
        }
        return nullptr;
    }

    /// Read every `wxITEM_CHECK` entry's initial state from the
    /// `config.commands` subtree (keyed by command name) and propagate
    /// it through the bound controls.
    void initializeCommands();

private:
    /// Catch-all wxEVT_MENU pre-dispatch. Keeps `wxITEM_CHECK` entries'
    /// `checked` flag in sync with the toolbar/menu toggle state.
    void onAnyEvent(wxCommandEvent& event);
    /// Mirror an AUI pane's close-via-X back into the matching command's
    /// `checked` state.
    void onAuiPaneClose(wxAuiManagerEvent& event);
    /// `New` — open a fresh untitled document.
    void onNew(wxCommandEvent& event);
    /// `Open` — show the open-file dialog.
    void onOpen(wxCommandEvent& event);
    /// `Save` — save the active document (falls through to Save As).
    void onSave(wxCommandEvent& event);
    /// `Save As` — prompt for a path then save.
    void onSaveAs(wxCommandEvent& event);
    /// `Save All` — save every modified document.
    void onSaveAll(wxCommandEvent& event);
    /// `Close` — close the active document tab.
    void onClose(wxCommandEvent& event);
    /// `Close All` — close every open document.
    void onCloseAll(wxCommandEvent& event);
    /// `New Window` — spawn a second FBIde process via `--new-window`.
    void onNewWindow(wxCommandEvent& event);
    /// `Quit` — close the main frame.
    void onQuit(wxCommandEvent& event);
    /// `Session → Load` — show the session-load dialog.
    void onSessionLoad(wxCommandEvent& event);
    /// `Session → Save` — show the session-save dialog.
    void onSessionSave(wxCommandEvent& event);
    /// Recent-files menu handler — opens the selected slot.
    void onFileHistory(wxCommandEvent& event);

    /// `Undo` on the active editor.
    void onUndo(wxCommandEvent& event);
    /// `Redo` on the active editor.
    void onRedo(wxCommandEvent& event);
    /// `Cut` on the active editor.
    void onCut(wxCommandEvent& event);
    /// `Copy` on the active editor.
    void onCopy(wxCommandEvent& event);
    /// `Paste` on the active editor.
    void onPaste(wxCommandEvent& event);
    /// `Select All` on the active editor.
    void onSelectAll(wxCommandEvent& event);
    /// `Select Line` — select the current line.
    void onSelectLine(wxCommandEvent& event);
    /// `Increase Indent` — Tab on selection.
    void onIndentIncrease(wxCommandEvent& event);
    /// `Decrease Indent` — Shift+Tab on selection.
    void onIndentDecrease(wxCommandEvent& event);
    /// `Comment` — prepend a comment marker to selected lines.
    void onComment(wxCommandEvent& event);
    /// `Uncomment` — strip the leading comment marker from selected lines.
    void onUncomment(wxCommandEvent& event);

    /// `Find` — show the Find dialog.
    void onFind(wxCommandEvent& event);
    /// `Find Next` — repeat the last search.
    void onFindNext(wxCommandEvent& event);
    /// `Replace` — show the Replace dialog.
    void onReplace(wxCommandEvent& event);
    /// `Goto Line` — show the goto-line dialog.
    void onGotoLine(wxCommandEvent& event);

    /// `Preferences` — open the Settings dialog.
    void onSettings(wxCommandEvent& event);
    /// `Format` — open the Format dialog for the active document.
    void onFormat(wxCommandEvent& event);
    /// `Show Subs` (F2) — reveal the Sub/Function browser.
    void onSubs(wxCommandEvent& event);
    /// `Compiler Log` — show the compiler-log dialog.
    void onCompilerLog(wxCommandEvent& event);

    /// `Compile` — kick off a compile of the active document.
    void onCompile(wxCommandEvent& event);
    /// `Compile and Run` — compile, then run on success.
    void onCompileAndRun(wxCommandEvent& event);
    /// `Run` — run the previously compiled executable.
    void onRun(wxCommandEvent& event);
    /// `Quick Run` — compile to a temp file and run.
    void onQuickRun(wxCommandEvent& event);
    /// `Kill Process` — abort the running build or program.
    void onKillProcess(wxCommandEvent& event);
    /// `Command Prompt` — open a terminal in the active doc's directory.
    void onCmdPrompt(wxCommandEvent& event);
    /// `Parameters` — prompt for runtime parameters.
    void onParameters(wxCommandEvent& event);

    /// `Help` — open the FreeBASIC help (CHM on Windows, web wiki elsewhere).
    void onHelp(wxCommandEvent& event);
    /// `About` — show the About dialog.
    void onAbout(wxCommandEvent& event);
    /// External-link menu handler — launches the registered URL.
    void onExternalLink(wxCommandEvent& event);

public:
    /// Drop every previously registered external link URL.
    /// Call before repopulating the Help menu's dynamic section.
    void clearExternalLinks();

    /// Reserve the next free external-link ID, remember the URL,
    /// and return the ID. Returns `wxID_ANY` if the range is exhausted.
    [[nodiscard]] auto registerExternalLink(const wxString& url) -> wxWindowID;

private:
    /// Application context — every handler dispatches through here.
    Context& m_ctx;
    /// Last value entered in the Parameters dialog.
    wxString m_parameters;
    /// Name → entry lookup table.
    std::unordered_map<wxString, CommandEntry> m_namedCommands;
    /// `wxWindowID` → name lookup, used to resolve dispatched events.
    std::unordered_map<wxWindowID, wxString> m_idNames;
    /// Dynamic external-link IDs (`ExternalLinkFirst`..`ExternalLinkLast`)
    /// mapped to the URL they should launch.
    std::unordered_map<wxWindowID, wxString> m_externalLinks;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
