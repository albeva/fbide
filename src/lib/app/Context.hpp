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
     * @param binaryPath Resolved directory of the running fbide binary
     *                   (where resources are located by default).
     * @param idePath    Override for the `<binary>/ide` resource directory
     *                   (`--ide` CLI flag).
     * @param configPath Override for the platform default config file
     *                   (`--config` CLI flag); relative paths resolve
     *                   against the IDE dir.
     */
    explicit Context(const wxString& binaryPath, const wxString& idePath = {}, const wxString& configPath = {});
    ~Context();

    [[nodiscard]] auto getConfigManager() -> ConfigManager& { return *m_configManager; }
    [[nodiscard]] auto getConfigManager() const -> const ConfigManager& { return *m_configManager; }

    /// Translate a locale path to a display string. Returns empty when
    /// the key is missing — never throws.
    [[nodiscard]] auto tr(const wxString& path) -> wxString;

    [[nodiscard]] auto getFileHistory() -> FileHistory& { return *m_fileHistory; }
    [[nodiscard]] auto getFileHistory() const -> const FileHistory& { return *m_fileHistory; }

    /// Editor theme. Forwarded from ConfigManager (theme is owned there,
    /// outside the Value tree because of its typed schema).
    [[nodiscard]] auto getTheme() -> Theme& { return m_configManager->getTheme(); }
    [[nodiscard]] auto getTheme() const -> const Theme& { return m_configManager->getTheme(); }

    [[nodiscard]] auto getUIManager() -> UIManager& { return *m_uiManager; }
    [[nodiscard]] auto getUIManager() const -> const UIManager& { return *m_uiManager; }

    [[nodiscard]] auto getSideBarManager() -> SideBarManager& { return *m_sideBarManager; }
    [[nodiscard]] auto getSideBarManager() const -> const SideBarManager& { return *m_sideBarManager; }

    [[nodiscard]] auto getDocumentManager() -> DocumentManager& { return *m_documentManager; }
    [[nodiscard]] auto getDocumentManager() const -> const DocumentManager& { return *m_documentManager; }

    [[nodiscard]] auto getFileSession() -> FileSession& { return *m_fileSession; }
    [[nodiscard]] auto getFileSession() const -> const FileSession& { return *m_fileSession; }

    [[nodiscard]] auto getCommandManager() -> CommandManager& { return *m_commandManager; }
    [[nodiscard]] auto getCommandManager() const -> const CommandManager& { return *m_commandManager; }

    [[nodiscard]] auto getCompilerManager() -> CompilerManager& { return *m_compilerManager; }
    [[nodiscard]] auto getCompilerManager() const -> const CompilerManager& { return *m_compilerManager; }

    [[nodiscard]] auto getHelpManager() -> HelpManager& { return *m_helpManager; }
    [[nodiscard]] auto getHelpManager() const -> const HelpManager& { return *m_helpManager; }

private:
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<FileHistory> m_fileHistory;
    std::unique_ptr<UIManager> m_uiManager;
    // SideBarManager is declared after UIManager so its destructor runs first
    // (members destroyed in reverse declaration order). It holds a non-owning
    // pointer to a wxAuiNotebook owned by the frame which UIManager destroys.
    std::unique_ptr<SideBarManager> m_sideBarManager;
    std::unique_ptr<DocumentManager> m_documentManager;
    std::unique_ptr<FileSession> m_fileSession;
    std::unique_ptr<CompilerManager> m_compilerManager;
    std::unique_ptr<HelpManager> m_helpManager;
    std::unique_ptr<CommandManager> m_commandManager;
};

} // namespace fbide
