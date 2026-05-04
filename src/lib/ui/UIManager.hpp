//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ArtiProvider.hpp"
#include "OutputConsole.hpp"
#include "UIState.hpp"
#include "command/CommandId.hpp"

namespace fbide {
class CompilerLog;
class Context;

/// RAII guard around `wxWindow::Freeze` / `Thaw`. Suppresses repaint
/// thrash during bulk UI updates; thaws on scope exit.
class [[nodiscard]] FreezeLock final {
public:
    NO_COPY_AND_MOVE(FreezeLock)

    /// Freeze `window` immediately if non-null. Thaws on destruction.
    explicit FreezeLock(wxWindow* window)
    : m_wnd(window) {
        if (window != nullptr) {
            window->Freeze();
        }
    }

    /// Thaw the held window if non-null.
    ~FreezeLock() {
        if (m_wnd != nullptr) {
            m_wnd->Thaw();
        }
    }

private:
    wxWindow* m_wnd; ///< Window to thaw on scope exit (nullable).
};

/**
 * Builds and owns the main application chrome: frame, menus, toolbar,
 * status bar, AUI dock, document notebook, sidebar notebook, and the
 * output console. Does not implement any command logic — handlers
 * live on `CommandManager`.
 *
 * **Owns:** every wx control attached to the main frame plus the
 * `ArtiProvider` and the `CompilerLog` dialog.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 * **State model:** carries two `UIState` slots — `m_documentState`
 * (set by DocumentManager from tab focus) and `m_compilerState`
 * (set by CompilerManager from build lifecycle). Compiler state
 * takes precedence; `applyState` translates the effective state into
 * `enabled` flags on `mutableIds[]`. See @ref commands.
 *
 * See @ref ui.
 */
class UIManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(UIManager)

    /// Construct without building any UI; `createMainFrame` does that later.
    explicit UIManager(Context& ctx);
    /// Destroy any lazily created chrome (compiler log dialog, etc.).
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

    /// Toggle the output console pane's visibility.
    void showConsole(bool show);

    /// Get the output console.
    [[nodiscard]] auto getOutputConsole() -> OutputConsole& { return *m_console; }

    /// Get the icon/bitmap provider.
    [[nodiscard]] auto getArtProvider() -> ArtiProvider& { return *m_artProvider; }
    /// Const overload of `getArtProvider`.
    [[nodiscard]] auto getArtProvider() const -> const ArtiProvider& { return *m_artProvider; }

    /// Get the compiler log dialog, creating it lazily if needed.
    [[nodiscard]] auto getCompilerLog() -> CompilerLog&;

    /// Freeze main window, returning object which will thaw when it goes out of scope
    [[nodiscard]] auto freeze() -> FreezeLock;

private:
    /// Set every command in `range` to disabled — helper for `applyState`.
    void disable(const std::ranges::range auto& range) const;

    /// Frame close — defers to `DocumentManager::prepareToQuit`.
    void onClose(wxCloseEvent& event);
    /// Notebook page close — route to `DocumentManager::closeFile`.
    void onPageClose(wxAuiNotebookEvent& event);
    /// Notebook page changed — refresh active document state.
    void onPageChanged(wxAuiNotebookEvent& event);
    /// Notebook double-click — open file dialog when clicking blank tab area.
    void onNotebookDblClick(wxAuiNotebookEvent& event);
    /// Status-bar click — open EOL/encoding pickers on the relevant fields.
    void onStatusBarClick(wxMouseEvent& event);

    /// Build the main menu bar from `layout.ini` + locale.
    void configureMenuBar();
    /// Recursively populate a single menu by id from layout.
    void configureMenuItems(wxMenu* menu, const wxString& id, bool addSeparators);
    /// Build the toolbar from `layout.ini`.
    void configureToolBar();
    /// Append the external-links submenu under Help.
    void generateExternalLinks(wxMenu* menu);

    /// Create the multi-field status bar.
    void createStatusBar() const;
    /// Create AUI panes, document notebook, sidebar notebook, output console.
    void createLayout();
    /// Sync the output-console pane's visibility with the `viewResult` command.
    void syncConsoleState(bool visible) const;
    /// Apply broad enable/disable for `mutableIds[]` based on `state`.
    void applyState(UIState state) const;
    /// Re-read system colours into every wxAUI art provider (dock,
    /// notebook tabs, toolbar). Called once after the layout is
    /// built; SetAppearance only re-paints native widgets, AUI's
    /// cached palette has to be refreshed explicitly.
    void refreshAuiArt() const;
    /// Capture the current frame size + position into `config["window"]`.
    void saveWindowGeometry();
    /// Serialise the current wxAUI pane layout into `config["aui"]["perspective"]`.
    /// Called on close after every document tab has been disposed of, so the
    /// stored layout reflects only the chrome (toolbars, sidebar, output) and
    /// no transient document state.
    void saveAuiPerspective();
    /// Apply a previously saved perspective string back onto `m_aui`. No-op if
    /// the config key is missing. Must run after every pane has been added so
    /// pane lookup by name succeeds.
    void loadAuiPerspective();

    Context& m_ctx;                                     ///< Application context.
    UIState m_documentState = UIState::None;            ///< Document-side state slot.
    UIState m_compilerState = UIState::None;            ///< Compiler-side state slot (overrides document).
    wxAuiManager m_aui;                                 ///< AUI dock manager for the frame.
    std::unique_ptr<ArtiProvider> m_artProvider;        ///< Icon/bitmap dispatch for menus + toolbar.
    Unowned<CompilerLog> m_compilerLog;                 ///< Compiler-log dialog (wx-parented, hidden until shown).
    Unowned<OutputConsole> m_console;                   ///< Build/run output pane.
    Unowned<wxFrame> m_frame;                           ///< Top-level frame.
    Unowned<wxToolBar> m_toolbar;                       ///< Classic frame toolbar (set when `toolbar.useAui=0`).
    Unowned<wxAuiToolBar> m_auiToolbar;                 ///< AUI-managed toolbar pane (set when `toolbar.useAui=1`).
    Unowned<wxAuiNotebook> m_notebook;                  ///< Document tabs.
    Unowned<wxAuiNotebook> m_sideBar;                   ///< Sidebar (Browser/Subs) notebook.
    std::vector<wxMenuItem*> m_externalLinkItems;       ///< Live menu items in the dynamic external-links submenu.

    // Document-level commands toggled by `applyState`. Edit commands here
    // (Undo, Redo, Cut, Copy, Paste, SelectAll) get their broad "is there
    // an editor" gate from applyState; DocumentManager::syncEditCommands
    // applies the fine-grained mask (CanUndo, has selection, clipboard,
    // etc.) via CommandEntry::setForceDisabled.
    /// Commands toggled by `applyState`. Edit commands here pick up their
    /// fine-grained mask separately via `DocumentManager::syncEditCommands`.
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
