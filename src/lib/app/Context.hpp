//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/ConfigManager.hpp"

namespace fbide {
class App;
class CommandManager;
class CompilerManager;
class DocumentManager;
class FileHistory;
class FileSession;
class HelpManager;
class SideBarManager;
class UIManager;

/**
 * Service locator for the application's long-lived managers.
 *
 * Constructed once on startup by `App` and destroyed last. Every manager
 * is owned via `unique_ptr` and reachable through a typed accessor; any
 * manager can reach any other through `Context&`. See @ref architecture
 * for the construction/destruction order and the threading map.
 *
 * **Owns:** every manager listed in the private section.
 * **Owned by:** `App` (lifetime tied to the application).
 * **Threading:** UI thread only. Managers may internally cross threads
 * (`IntellisenseService`); Context itself does not.
 * **Lifecycle:** constructed in `App::OnInit` after CLI parsing;
 * destroyed in `App::OnExit` reverse-order destruction. The field
 * order in this header is the exact construction order.
 */
class Context final {
public:
    NO_COPY_AND_MOVE(Context)

    /**
     * Initialise the context tree.
     *
     * @param app        Owning application — pinned by reference so any
     *                   manager can call back into App via `getApp()`.
     * @param binaryPath Resolved directory of the running fbide binary
     *                   (where resources are located by default).
     * @param idePath    Override for the `<binary>/ide` resource directory
     *                   (`--ide` CLI flag).
     * @param configPath Override for the platform default config file
     *                   (`--config` CLI flag); relative paths resolve
     *                   against the IDE dir.
     */
    explicit Context(App& app, const wxString& binaryPath, const wxString& idePath = {}, const wxString& configPath = {});
    ~Context();

    /// Owning application.
    [[nodiscard]] auto getApp() -> App& { return m_app; }
    /// Const overload of `getApp`.
    [[nodiscard]] auto getApp() const -> const App& { return m_app; }

    /// Access the configuration manager.
    [[nodiscard]] auto getConfigManager() -> ConfigManager& { return *m_configManager; }
    /// Const overload of `getConfigManager`.
    [[nodiscard]] auto getConfigManager() const -> const ConfigManager& { return *m_configManager; }

    /// Translate a locale path to a display string. Returns empty when
    /// the key is missing — never throws.
    [[nodiscard]] auto tr(const wxString& path) -> wxString;

    /// Access the recent-files store.
    [[nodiscard]] auto getFileHistory() -> FileHistory& { return *m_fileHistory; }
    /// Const overload of `getFileHistory`.
    [[nodiscard]] auto getFileHistory() const -> const FileHistory& { return *m_fileHistory; }

    /// Editor theme. Forwarded from ConfigManager (theme is owned there,
    /// outside the Value tree because of its typed schema).
    [[nodiscard]] auto getTheme() -> Theme& { return m_configManager->getTheme(); }
    /// Const overload of `getTheme`.
    [[nodiscard]] auto getTheme() const -> const Theme& { return m_configManager->getTheme(); }

    /// Access the UI manager (frame, menus, toolbar, status bar).
    [[nodiscard]] auto getUIManager() -> UIManager& { return *m_uiManager; }
    /// Const overload of `getUIManager`.
    [[nodiscard]] auto getUIManager() const -> const UIManager& { return *m_uiManager; }

    /// Access the sidebar (Browser / Subs) manager.
    [[nodiscard]] auto getSideBarManager() -> SideBarManager& { return *m_sideBarManager; }
    /// Const overload of `getSideBarManager`.
    [[nodiscard]] auto getSideBarManager() const -> const SideBarManager& { return *m_sideBarManager; }

    /// Access the document manager (open files, tabs, find/replace).
    [[nodiscard]] auto getDocumentManager() -> DocumentManager& { return *m_documentManager; }
    /// Const overload of `getDocumentManager`.
    [[nodiscard]] auto getDocumentManager() const -> const DocumentManager& { return *m_documentManager; }

    /// Access the session manager (`.fbs` load/save).
    [[nodiscard]] auto getFileSession() -> FileSession& { return *m_fileSession; }
    /// Const overload of `getFileSession`.
    [[nodiscard]] auto getFileSession() const -> const FileSession& { return *m_fileSession; }

    /// Access the command manager (menu/toolbar dispatch).
    [[nodiscard]] auto getCommandManager() -> CommandManager& { return *m_commandManager; }
    /// Const overload of `getCommandManager`.
    [[nodiscard]] auto getCommandManager() const -> const CommandManager& { return *m_commandManager; }

    /// Access the compiler manager (compile/run/quickrun).
    [[nodiscard]] auto getCompilerManager() -> CompilerManager& { return *m_compilerManager; }
    /// Const overload of `getCompilerManager`.
    [[nodiscard]] auto getCompilerManager() const -> const CompilerManager& { return *m_compilerManager; }

    /// Access the help manager (CHM viewer / wiki fallback).
    [[nodiscard]] auto getHelpManager() -> HelpManager& { return *m_helpManager; }
    /// Const overload of `getHelpManager`.
    [[nodiscard]] auto getHelpManager() const -> const HelpManager& { return *m_helpManager; }

private:
    App& m_app;                                      ///< Owning application.
    std::unique_ptr<ConfigManager> m_configManager;  ///< INI store + path resolver.
    std::unique_ptr<FileHistory> m_fileHistory;      ///< Recent-files list.
    std::unique_ptr<UIManager> m_uiManager;          ///< Main frame + chrome.
    // SideBarManager is declared after UIManager so its destructor runs first
    // (members destroyed in reverse declaration order). It holds a non-owning
    // pointer to a wxAuiNotebook owned by the frame which UIManager destroys.
    std::unique_ptr<SideBarManager> m_sideBarManager;  ///< Browser/Subs sidebar.
    std::unique_ptr<DocumentManager> m_documentManager;///< Open documents + tabs.
    std::unique_ptr<FileSession> m_fileSession;        ///< Session `.fbs` load/save.
    std::unique_ptr<CompilerManager> m_compilerManager;///< Compile/run lifecycle.
    std::unique_ptr<HelpManager> m_helpManager;        ///< Help dispatcher.
    std::unique_ptr<CommandManager> m_commandManager;  ///< Command table + dispatch (last).
};

} // namespace fbide
