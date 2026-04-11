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
class Lang;

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

private:
    std::unique_ptr<Config> m_config;
    std::unique_ptr<Lang> m_lang;
};

} // namespace fbide
