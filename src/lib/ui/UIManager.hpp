//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "OutputConsole.hpp"

namespace fbide {
class CompilerLog;
class Context;

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

    /// Enable/disable editor-dependent menus and toolbar items.
    void enableEditorMenus(bool state) const;

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

    /// Enable/disable compile/run toolbar and menu items.
    void enableRunMenus(bool state) const;

    /// Get the compiler log dialog, creating it lazily if needed.
    [[nodiscard]] auto getCompilerLog() -> CompilerLog&;

private:
    void onClose(wxCloseEvent& event);
    void onPageClose(wxAuiNotebookEvent& event);
    void onPageChanged(wxAuiNotebookEvent& event);

    void createMenuBar();
    void createToolBar();
    void createStatusBar() const;
    void createLayout();
    void syncConsoleState(bool visible) const;

    Context& m_ctx;
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
};

} // namespace fbide
