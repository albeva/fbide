//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ArtiProvider.hpp"
#include "command/CommandId.hpp"
#include "OutputConsole.hpp"
#include "UIState.hpp"

namespace fbide {
class CompilerLog;
class Context;

class [[nodiscard]] FreezeLock final {
public:
    NO_COPY_AND_MOVE(FreezeLock)

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

    /// Re-run menu and toolbar configuration. Call after locale or layout
    /// changes so labels, shortcuts and command entries refresh in place.
    void refreshUi();

    // /// Show the console pane if not already visible.
    void showConsole(bool show);

    /// Get the output console.
    [[nodiscard]] auto getOutputConsole() -> OutputConsole& { return *m_console; }

    /// Get the icon/bitmap provider.
    [[nodiscard]] auto getArtProvider() -> ArtiProvider& { return *m_artProvider; }
    [[nodiscard]] auto getArtProvider() const -> const ArtiProvider& { return *m_artProvider; }

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

    void configureMenuBar();
    void configureMenuItems(wxMenu* menu, const wxString& id, bool addSeparators);
    void configureToolBar();
    void generateExternalLinks(wxMenu* menu);

    void createStatusBar() const;
    void createLayout();
    void syncConsoleState(bool visible) const;
    void applyState(UIState state) const;

    Context& m_ctx;
    UIState m_documentState = UIState::None;
    UIState m_compilerState = UIState::None;
    wxAuiManager m_aui;
    std::unique_ptr<ArtiProvider> m_artProvider;
    CompilerLog* m_compilerLog = nullptr;
    Unowned<OutputConsole> m_console;
    Unowned<wxFrame> m_frame;
    Unowned<wxToolBar> m_toolbar;
    Unowned<wxAuiNotebook> m_notebook;
    std::vector<wxMenuItem*> m_externalLinkItems;

    static constexpr std::array mutableIds = {
        CommandId::Save,
        CommandId::SaveAs,
        CommandId::Close,
        CommandId::Undo,
        CommandId::Redo,
        CommandId::Cut,
        CommandId::Copy,
        CommandId::Paste,
        CommandId::SelectAll,
        CommandId::Find,
        CommandId::Replace,
        CommandId::SaveAll,
        CommandId::SessionSave,
        CommandId::CloseAll,
        CommandId::SelectLine,
        CommandId::IndentIncrease,
        CommandId::IndentDecrease,
        CommandId::Comment,
        CommandId::Uncomment,
        CommandId::FindNext,
        CommandId::FindPrevious,
        CommandId::GotoLine,
        CommandId::Format,
        // CommandId::Result,
        CommandId::CompilerLog,
        CommandId::Subs,
        CommandId::Compile,
        CommandId::CompileAndRun,
        CommandId::Run,
        CommandId::QuickRun,
        CommandId::KillProcess,
        CommandId::CmdPrompt,
        CommandId::Parameters,
        CommandId::ShowExitCode
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
