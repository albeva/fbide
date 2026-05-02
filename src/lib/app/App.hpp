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

/**
 * Application entry point. Drives the startup pipeline: parse CLI,
 * configure logging, build `Context`, handle `--cfg` / single-instance
 * forwarding, then create the main frame and open positional files.
 *
 * **Owns:** the singular `Context` and the conditional
 * `InstanceHandler` (skipped under `--new-window`).
 * **Threading:** UI thread only.
 *
 * See @ref architecture for the full lifecycle.
 */
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
    /// Parsed command-line state. Filled by `parseCli` once at startup so the
    /// rest of `OnInit` can branch on it without re-parsing.
    struct CliOptions {
        wxString configPath;           // --config <path>
        wxString idePath;              // --ide <path>
        wxString cfgKey;               // --cfg=[<category>:]<key> (non-empty → print + exit)
        wxArrayString files;           // positional file paths
        bool newWindow = false;        // --new-window
        bool verbose = false;          // --verbose
        bool helpRequested = false;    // --help
        bool versionRequested = false; // --version
        bool parseFailed = false;
    };

    /// Get the directory of the fbide binary.
    [[nodiscard]] auto getFbidePath() -> wxString;

    /// Parse command-line arguments. Pure: doesn't touch app state.
    [[nodiscard]] auto parseCli() const -> CliOptions;

    /// Print usage to stdout (attaching the parent console on Windows so
    /// the text reaches the launching shell).
    void showHelp() const;

    /// Print fbide + wxWidgets version to stdout.
    void showVersion() const;

    /// Resolve `--cfg=<spec>` (where `<spec>` is `[category:]dotted.key`).
    /// Returns the value as a string, or empty if missing.
    [[nodiscard]] auto resolveCfg(const wxString& spec) const -> wxString;

    /// Show splash screen if enabled.
    void showSplash();

    /// Open files passed on the command line or via OS events.
    void openFiles(const wxArrayString& files);

    std::unique_ptr<Context> m_context;
    std::unique_ptr<InstanceHandler> m_instanceHandler;
    bool m_newWindow = false;
};

} // namespace fbide
