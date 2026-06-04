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
#include "StatusBarHandler.hpp"
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

    /// Access the application context — for widgets that already hold a
    /// `UIManager*` and need localisation lookups (status bar, editor).
    [[nodiscard]] auto getContext() const -> Context& { return m_ctx; }

    /// Build the main application frame with all UI elements.
    void createMainFrame();

    /// Get the main frame.
    [[nodiscard]] auto getMainFrame() -> wxFrame* { return m_frame; }

    /// Recompute the enable / disable state of non-build mutable
    /// commands based on the active document. Disables everything
    /// when no document is active; disables FB-only edit operations
    /// (Comment / Uncomment / Format / Subs) when the active document
    /// is not FreeBASIC. Also chains into `DocumentManager::syncEditCommands`
    /// for the fine-grained Undo / Redo / Cut / Copy / Paste / SelectAll mask.
    void syncDocCommands();

    /// Recompute the enable / disable state of build commands
    /// (Compile / CompileAndRun / Run / QuickRun / KillProcess) from
    /// the active `Project`'s capabilities. When the compiler state is
    /// `Compiling` or `Running`, all build commands are frozen and
    /// `KillProcess` is enabled regardless of capabilities.
    void syncBuildCommands();

    /// Set the compiler-level UI state (None, Compiling, Running).
    /// Drives status-bar feedback for long-running jobs and overrides
    /// `syncBuildCommands` to freeze the build set while a process is
    /// in flight.
    void setCompilerState(UIState state);

    /// Force editors to update settings.
    void updateSettings();

    /// Toggle the output console pane's visibility.
    void showConsole(bool show);

    /// Apply the current `commands.configurationInStatusBar` preference:
    /// adjust the status-bar field count and add or remove the toolbar
    /// combobox (rebuilding the toolbar when a runtime toggle leaves it
    /// out of step). Called once at startup and again from
    /// `updateSettings` so changes take effect without a restart.
    void refreshConfigurationDisplay();

    /// Access the status-bar handler — used by `Editor` to push line
    /// / column + per-document fields, and by `CompilerManager` to
    /// refresh the configuration label on catalog changes.
    [[nodiscard]] auto getStatusBar() -> StatusBarHandler& { return m_statusBar; }
    [[nodiscard]] auto getStatusBar() const -> const StatusBarHandler& { return m_statusBar; }

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

    /// Set window title. This is prefixed with "FBIde - ", when non empty.
    void setTitle(const wxString& title);

    /// Wipe every document-scoped status-bar field (line:col, type, EOL,
    /// encoding). The field-index schema lives in `createStatusBar` and
    /// shouldn't leak to callers; `DocumentManager::closeFile` calls
    /// this when the last tab closes.
    void clearDocumentStatus();

private:
    /// Set the enabled flag on a single command. No-op when the
    /// command id has no registered entry.
    void setEnabled(CommandId id, bool enabled) const;

    /// Frame close — defers to `DocumentManager::prepareToQuit`.
    void onClose(wxCloseEvent& event);
    /// Build the main menu bar from `layout.ini` + locale.
    void configureMenuBar();
    /// Recursively populate a single menu by id from layout.
    void configureMenuItems(wxMenu* menu, const wxString& id, bool addSeparators);
    /// Build the toolbar from `layout.ini`. Re-entrant: a second call
    /// rebuilds in place (used when the configuration combobox has to be
    /// added or removed after a preference toggle).
    void configureToolBar();
    /// Append the external-links submenu under Help.
    void generateExternalLinks(wxMenu* menu);

    /// Create the multi-field status bar by delegating to
    /// `StatusBarHandler`.
    void createStatusBar();
    /// Create AUI panes, document notebook, sidebar notebook, output console.
    void createLayout();
    /// Sync the output-console pane's visibility with the `viewResult` command.
    void syncConsoleState(bool visible) const;
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
    void resetToolbarSize();

    Context& m_ctx;                               ///< Application context.
    UIState m_compilerState = UIState::None;      ///< Compiler-side state slot (overrides build capabilities).
    wxAuiManager m_aui;                           ///< AUI dock manager for the frame.
    StatusBarHandler m_statusBar;                 ///< Status-bar field layout, content, and click routing.
    std::unique_ptr<ArtiProvider> m_artProvider;  ///< Icon/bitmap dispatch for menus + toolbar.
    Unowned<CompilerLog> m_compilerLog;           ///< Compiler-log dialog (wx-parented, hidden until shown).
    Unowned<OutputConsole> m_console;             ///< Build/run output pane.
    Unowned<wxFrame> m_frame;                     ///< Top-level frame.
    Unowned<wxAuiToolBar> m_auiToolbar;           ///< AUI-managed toolbar pane.
    Unowned<wxAuiNotebook> m_sideBar;             ///< Sidebar (Browser/Subs) notebook.
    std::vector<wxMenuItem*> m_externalLinkItems; ///< Live menu items in the dynamic external-links submenu.

    // Document-level commands toggled by `syncDocCommands`. Edit commands
    // here (Undo, Redo, Cut, Copy, Paste, SelectAll) get their broad "is
    // there an editor" gate from syncDocCommands; `CommandManager::syncEditCommands`
    // applies the fine-grained mask (CanUndo, has selection, clipboard,
    // etc.) via `CommandEntry::setForceDisabled`.
    //
    // Build commands (Compile / CompileAndRun / Run / QuickRun /
    // KillProcess) are intentionally NOT here — they are driven by
    // `syncBuildCommands` from the active `Project`'s capabilities
    // plus `m_compilerState`. FB-only edit operations (Comment /
    // Uncomment / Format / Subs) live in `kFreeBasicEditCommandIds`
    // below and get an extra disable pass when the active document
    // is not FreeBASIC.
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
        CommandId::CmdPrompt,
        CommandId::Parameters,
        CommandId::ShowExitCode,
    };

    /// FB-only edit operations — disabled when the active document
    /// isn't FreeBASIC. Subset of `mutableIds`.
    static constexpr std::array kFreeBasicEditCommandIds = {
        CommandId::Comment,
        CommandId::Uncomment,
        CommandId::Format,
        CommandId::Subs,
    };

    /// Build / run command set — driven by `syncBuildCommands` rather
    /// than by `mutableIds` membership, so the active project's
    /// capabilities can gate each one independently.
    static constexpr std::array kBuildCommandIds = {
        CommandId::Compile,
        CommandId::CompileAndRun,
        CommandId::Run,
        CommandId::QuickRun,
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
