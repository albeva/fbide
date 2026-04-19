//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Context.hpp"
#include "InstanceHandler.hpp"

namespace fbide {

/// Main application class. Manages the application lifecycle.
class App final : public wxApp {
public:
    NO_COPY_AND_MOVE(App)
    App() = default;

    /// Initialize the application, create main window.
    auto OnInit() -> bool override;

    /// Cleanup on exit — flush clipboard so copied content persists after app closes.
    auto OnExit() -> int override;
    void initAppearance();

    /// Get the application context.
    [[nodiscard]] auto getContext() -> Context& { return *m_context; }
    [[nodiscard]] auto getContext() const -> const Context& { return *m_context; }

private:
    /// Get the directory of the fbide binary.
    [[nodiscard]] auto getFbidePath() -> wxString;

    /// Parse command line arguments into config file and files to open.
    void parseArgs(wxString& configFile, wxArrayString& filesToOpen);

    /// Show splash screen if enabled.
    void showSplash();

    /// Open files passed on the command line or via OS events.
    void openFiles(const wxArrayString& files);

    std::unique_ptr<Context> m_context;
    std::unique_ptr<InstanceHandler> m_instanceHandler;
    bool m_newWindow = false;
};

} // namespace fbide
