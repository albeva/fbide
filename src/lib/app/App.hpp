//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Context.hpp"

namespace fbide {

/// Main application class. Manages the application lifecycle.
class App final : public wxApp {
public:
    /// Initialize the application, create main window.
    auto OnInit() -> bool override;

    /// Get the application context.
    [[nodiscard]] auto getContext() -> Context& { return *m_context; }
    [[nodiscard]] auto getContext() const -> const Context& { return *m_context; }

private:
    /// Get the directory of teh fbide binary
    [[nodiscard]] auto getFbidePath() -> wxString;

    std::unique_ptr<Context> m_context;
};

} // namespace fbide
