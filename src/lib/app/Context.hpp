//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Config;
class Keywords;
class Lang;
class Theme;
class UIManager;

/// Central access point for editor service objects.
/// Owns Config, Lang, and other shared state.
/// Created first, destroyed last.
class Context final {
public:
    /// Initialize context with resolved binary path.
    explicit Context(const wxString& binaryPath);
    ~Context();

    Context(const Context&) = delete;
    auto operator=(const Context&) -> Context& = delete;
    Context(Context&&) = delete;
    auto operator=(Context&&) -> Context& = delete;

    /// Get application configuration.
    [[nodiscard]] auto getConfig() -> Config& { return *m_config; }
    [[nodiscard]] auto getConfig() const -> const Config& { return *m_config; }

    /// Get translation strings.
    [[nodiscard]] auto getLang() -> Lang& { return *m_lang; }
    [[nodiscard]] auto getLang() const -> const Lang& { return *m_lang; }

    /// Get syntax keywords.
    [[nodiscard]] auto getKeywords() -> Keywords& { return *m_keywords; }
    [[nodiscard]] auto getKeywords() const -> const Keywords& { return *m_keywords; }

    /// Get editor theme.
    [[nodiscard]] auto getTheme() -> Theme& { return *m_theme; }
    [[nodiscard]] auto getTheme() const -> const Theme& { return *m_theme; }

    /// Get UI manager.
    [[nodiscard]] auto getUIManager() -> UIManager& { return *m_uiManager; }
    [[nodiscard]] auto getUIManager() const -> const UIManager& { return *m_uiManager; }

private:
    std::unique_ptr<Config> m_config;
    std::unique_ptr<Keywords> m_keywords;
    std::unique_ptr<Lang> m_lang;
    std::unique_ptr<Theme> m_theme;
    std::unique_ptr<UIManager> m_uiManager;
};

} // namespace fbide
