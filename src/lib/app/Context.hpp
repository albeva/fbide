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

/// Central access point for editor service objects.
/// Owns ConfigManager and other shared state.
/// Created first, destroyed last.
class Context final {
public:
    NO_COPY_AND_MOVE(Context)

    /// Initialize context with resolved binary path. Optional `idePath`
    /// overrides the default `<binary>/ide` resource directory; optional
    /// `configPath` overrides the platform default config file (resolved
    /// relative to the IDE dir when not absolute).
    explicit Context(const wxString& binaryPath, const wxString& idePath = {}, const wxString& configPath = {});
    ~Context();

    [[nodiscard]] auto getConfigManager() -> ConfigManager& { return *m_configManager; }
    [[nodiscard]] auto getConfigManager() const -> const ConfigManager& { return *m_configManager; }

    /// Translate a locale path to a display string (ConfigManager::locale_or with empty default).
    [[nodiscard]] auto tr(const wxString& path) -> wxString;

    /// Get file history.
    [[nodiscard]] auto getFileHistory() -> FileHistory& { return *m_fileHistory; }
    [[nodiscard]] auto getFileHistory() const -> const FileHistory& { return *m_fileHistory; }

    /// Get editor theme (owned by ConfigManager).
    [[nodiscard]] auto getTheme() -> Theme& { return m_configManager->getTheme(); }
    [[nodiscard]] auto getTheme() const -> const Theme& { return m_configManager->getTheme(); }

    /// Get UI manager.
    [[nodiscard]] auto getUIManager() -> UIManager& { return *m_uiManager; }
    [[nodiscard]] auto getUIManager() const -> const UIManager& { return *m_uiManager; }

    /// Get sidebar (Browser) manager.
    [[nodiscard]] auto getSideBarManager() -> SideBarManager& { return *m_sideBarManager; }
    [[nodiscard]] auto getSideBarManager() const -> const SideBarManager& { return *m_sideBarManager; }

    /// Get document manager.
    [[nodiscard]] auto getDocumentManager() -> DocumentManager& { return *m_documentManager; }
    [[nodiscard]] auto getDocumentManager() const -> const DocumentManager& { return *m_documentManager; }

    /// Get session manager (handles .fbs load/save).
    [[nodiscard]] auto getFileSession() -> FileSession& { return *m_fileSession; }
    [[nodiscard]] auto getFileSession() const -> const FileSession& { return *m_fileSession; }

    /// Get command manager.
    [[nodiscard]] auto getCommandManager() -> CommandManager& { return *m_commandManager; }
    [[nodiscard]] auto getCommandManager() const -> const CommandManager& { return *m_commandManager; }

    /// Get compiler manager.
    [[nodiscard]] auto getCompilerManager() -> CompilerManager& { return *m_compilerManager; }
    [[nodiscard]] auto getCompilerManager() const -> const CompilerManager& { return *m_compilerManager; }

    /// Get help manager.
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
