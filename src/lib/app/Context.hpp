//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class CommandManager;
class CompilerManager;
class Config;
class ConfigManager;
class DocumentManager;
class FileHistory;
class HelpManager;
class ThemeOld;
class UIManager;

/// Central access point for editor service objects.
/// Owns Config and other shared state.
/// Created first, destroyed last.
class Context final {
public:
    NO_COPY_AND_MOVE(Context)

    /// Initialize context with resolved binary path.
    explicit Context(const wxString& binaryPath);
    ~Context();

    /// Get application configuration.
    [[nodiscard]] auto getConfig() -> Config& { return *m_config; }
    [[nodiscard]] auto getConfig() const -> const Config& { return *m_config; }

    [[nodiscard]] auto getConfigManager() -> ConfigManager& { return *m_configManager; }
    [[nodiscard]] auto getConfigManager() const -> const ConfigManager& { return *m_configManager; }

    /// Translate a locale path to a display string (ConfigManager::locale_or with empty default).
    [[nodiscard]] auto tr(const wxString& path) -> wxString;

    /// Get file history.
    [[nodiscard]] auto getFileHistory() -> FileHistory& { return *m_fileHistory; }
    [[nodiscard]] auto getFileHistory() const -> const FileHistory& { return *m_fileHistory; }

    /// Get editor theme.
    [[nodiscard]] auto getTheme() -> ThemeOld& { return *m_theme; }
    [[nodiscard]] auto getTheme() const -> const ThemeOld& { return *m_theme; }

    /// Get UI manager.
    [[nodiscard]] auto getUIManager() -> UIManager& { return *m_uiManager; }
    [[nodiscard]] auto getUIManager() const -> const UIManager& { return *m_uiManager; }

    /// Get document manager.
    [[nodiscard]] auto getDocumentManager() -> DocumentManager& { return *m_documentManager; }
    [[nodiscard]] auto getDocumentManager() const -> const DocumentManager& { return *m_documentManager; }

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
    std::unique_ptr<Config> m_config;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<FileHistory> m_fileHistory;
    std::unique_ptr<ThemeOld> m_theme;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<DocumentManager> m_documentManager;
    std::unique_ptr<CompilerManager> m_compilerManager;
    std::unique_ptr<HelpManager> m_helpManager;
    std::unique_ptr<CommandManager> m_commandManager;
};

} // namespace fbide
