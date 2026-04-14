//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "MenuId.hpp"
#include "OutputConsole.hpp"
#include "UIState.hpp"

namespace fbide {
class CompilerLog;
class Context;

class [[nodiscard]] FreezeLock final {
public:
    NO_COPY_AND_MOVE(FreezeLock);

    explicit FreezeLock(wxWindow* window)
    : m_wnd(window) {
        if (window != nullptr) {
            window->Freeze();
        }
    }

    ~FreezeLock() {
        if (m_wnd != nullptr) {
            m_wnd->Thaw();
        }
    }

private:
    wxWindow* m_wnd;
};

/// Manages the main application UI: frame, menus, toolbar, statusbar, layout.
/// Does not handle command logic — that is CommandManager's responsibility.
class UIManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(UIManager)

    explicit UIManager(Context& ctx);
    ~UIManager() override;

    /// Build the main application frame with all UI elements.
    void createMainFrame();

    /// Get the main frame.
    [[nodiscard]] auto getMainFrame() -> wxFrame* { return m_frame; }

    /// Get the document notebook.
    [[nodiscard]] auto getNotebook() -> wxAuiNotebook* { return m_notebook; }

    /// Set the document-level UI state (None, FocusedUnknownFile, FocusedValidSourceFile).
    /// Compiler state takes precedence when active.
    void setDocumentState(UIState state);

    /// Set the compiler-level UI state (None, Compiling, Running).
    /// When not None, overrides the document state.
    void setCompilerState(UIState state);

    /// Force editors to update settings.
    void updateEditorSettigs();

    /// Toggle the console/output pane visibility.
    void toggleConsole();

    /// Show the console pane if not already visible.
    void showConsole();

    /// Hide the console pane if visible.
    void hideConsole();

    /// Is the console pane visible?
    [[nodiscard]] auto isConsoleVisible() -> bool;

    /// Get the output console.
    [[nodiscard]] auto getOutputConsole() -> OutputConsole& { return *m_console; }

    /// Get the compiler log dialog, creating it lazily if needed.
    [[nodiscard]] auto getCompilerLog() -> CompilerLog&;

    /// Freeze main window, returning object which will thaw when it goes out of scope
    [[nodiscard]] auto freeze() -> FreezeLock;

private:
    void disable(const std::ranges::range auto& range) const;

    void onClose(wxCloseEvent& event);
    void onPageClose(wxAuiNotebookEvent& event);
    void onPageChanged(wxAuiNotebookEvent& event);
    void onNotebookDblClick(wxAuiNotebookEvent& event);

    void createMenuBar();
    void createToolBar();
    void createStatusBar() const;
    void createLayout();
    void syncConsoleState(bool visible) const;
    void applyState(UIState state) const;

    Context& m_ctx;
    UIState m_documentState = UIState::None;
    UIState m_compilerState = UIState::None;
    wxAuiManager m_aui;
    CompilerLog* m_compilerLog = nullptr;
    Unowned<OutputConsole> m_console;
    Unowned<wxFrame> m_frame;
    Unowned<wxToolBar> m_toolbar;
    Unowned<wxAuiNotebook> m_notebook;
    Unowned<wxMenu> m_fileMenu;
    Unowned<wxMenu> m_editMenu;
    Unowned<wxMenu> m_searchMenu;
    Unowned<wxMenu> m_viewMenu;
    Unowned<wxMenu> m_runMenu;
    Unowned<wxMenu> m_helpMenu;

    static constexpr std::array mutableIds = {
        MenuId::Save,
        MenuId::SaveAs,
        MenuId::Close,
        MenuId::Undo,
        MenuId::Redo,
        MenuId::Cut,
        MenuId::Copy,
        MenuId::Paste,
        MenuId::SelectAll,
        MenuId::Find,
        MenuId::Replace,
        MenuId::SaveAll,
        MenuId::SessionSave,
        MenuId::CloseAll,
        MenuId::SelectLine,
        MenuId::IndentIncrease,
        MenuId::IndentDecrease,
        MenuId::Comment,
        MenuId::Uncomment,
        MenuId::FindNext,
        MenuId::FindPrevious,
        MenuId::GotoLine,
        MenuId::Format,
        MenuId::Result,
        MenuId::CompilerLog,
        MenuId::Subs,
        MenuId::Compile,
        MenuId::CompileAndRun,
        MenuId::Run,
        MenuId::QuickRun,
        MenuId::CmdPrompt,
        MenuId::Parameters,
        MenuId::ShowExitCode
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
