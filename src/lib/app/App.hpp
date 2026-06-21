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
    /// Default constructor — no work; `OnInit` does the heavy lifting.
    App() = default;

    /// Initialize the application, create main window.
    auto OnInit() -> bool override;

    /// Cleanup on exit — flush clipboard so copied content persists after app closes.
    auto OnExit() -> int override;

#ifdef __WXOSX__
    /// Receive document-open events from the OS
    void MacOpenFiles(const wxArrayString& fileNames) override;
#endif

    /// Apply `appearance=` config (light/dark/system) to the wx appearance API.
    void initAppearance();

    /// Get the application context.
    [[nodiscard]] auto getContext() -> Context& { return *m_context; }
    /// Const overload of `getContext`.
    [[nodiscard]] auto getContext() const -> const Context& { return *m_context; }

    /// Schedule a deferred relaunch of FBIde. The lambda runs on the
    /// next event-loop tick: the open documents are saved to a temp
    /// session file, `commitConfig` (if any) is invoked once that
    /// save succeeds, and a replacement process is spawned with
    /// `--wait-for-pid` so it blocks until this one has exited. The
    /// current frame is then closed via the normal EVT_CLOSE chain,
    /// which persists window geometry / config / file history on its
    /// way out.
    ///
    /// `commitConfig` is the place to apply any in-memory config
    /// changes that should only land when the restart actually goes
    /// through (e.g. swapping the locale path for a language change):
    /// if the user cancels an in-flight save dialog, FileSession
    /// returns false and `commitConfig` is never called.
    void scheduleRestart(std::function<void()> commitConfig = {});

    /// Queue files (from the command line, the IPC server, or OS
    /// events) and open any the notebook can take right now. Opens that
    /// arrive before `createMainFrame()` — e.g. a second-instance
    /// forward during the splash screen — stay queued until OnInit
    /// drains them once the frame exists.
    void openFiles(const wxArrayString& files);

private:
    /// Parsed command-line state. Filled by `parseCli` once at startup so the
    /// rest of `OnInit` can branch on it without re-parsing.
    struct CliOptions {
        wxString configPath;           ///< `--config <path>`.
        wxString idePath;              ///< `--ide <path>`.
        wxString logPath;              ///< `--log-path <path>` (empty → user data dir default).
        wxString cfgKey;               ///< `--cfg=[<category>:]<key>` (non-empty → print + exit).
        wxString restoreStateFrom;     ///< `--restore-state-from <path>`.
        wxArrayString files;           ///< Positional file paths.
        // `format <file>` subcommand — `formatRequested` set → format + exit.
        bool formatRequested = false;  ///< `format` subcommand seen.
        wxString formatInput;          ///< Input file to format.
        wxString formatOutput;         ///< `-o/--output <file>` (empty → stdout).
        bool formatReindent = false;   ///< `--reindent`.
        bool formatReformat = false;   ///< `--reformat`.
        bool formatAlignPP = false;    ///< `--align-pp`.
        bool formatApplyCase = false;  ///< `--apply-case`.
        bool formatHtml = false;       ///< `--html`.
        int waitForPid = 0;            ///< `--wait-for-pid <pid>` (0 = no wait).
        bool newWindow = false;        ///< `--new-window`.
        bool skipWarmDefines = false;  ///< `--skip-warm-defines` (diagnostic: skip startup fbc probe).
        bool verbose = false;          ///< `--verbose`.
        bool helpRequested = false;    ///< `--help`.
        bool versionRequested = false; ///< `--version`.
        bool parseFailed = false;      ///< Set when CLI parsing reported an error to stderr.
    };

    /// Get the directory of the fbide binary.
    [[nodiscard]] auto getFbidePath() -> wxString;

    /// Parse command-line arguments. Pure: doesn't touch app state.
    [[nodiscard]] auto parseCli() const -> CliOptions;

    /// Print usage to stdout (attaching the parent console on Windows so
    /// the text reaches the launching shell).
    static void showHelp();

    /// Print fbide + wxWidgets version to stdout.
    static void showVersion();

    /// Resolve `--cfg=<spec>` (where `<spec>` is `[category:]dotted.key`).
    /// Returns the value as a string, or empty if missing.
    [[nodiscard]] auto resolveCfg(const wxString& spec) const -> wxString;

    /// Show splash screen if enabled.
    void showSplash() const;

    std::unique_ptr<Context> m_context;                 ///< Application service locator.
    std::unique_ptr<std::ofstream> m_logStream;         ///< Backing file for the wxLogStream target (which borrows it).
    wxArrayString m_pendingFiles;                       ///< Files awaiting an open until the main frame exists.
    std::unique_ptr<InstanceHandler> m_instanceHandler; ///< Single-instance gate (skipped under `--new-window`).
    bool m_newWindow = false;                           ///< Effective value of `--new-window`.
    bool m_verbose = false;                             ///< Effective value of `--verbose` — replayed on relaunch.
    wxString m_configPath;                              ///< `--config <path>` value, if any — replayed on relaunch.
    wxString m_idePath;                                 ///< `--ide <path>` value, if any — replayed on relaunch.
    wxString m_logPath;                                 ///< `--log-path <path>` value, if any — replayed on relaunch.
};

} // namespace fbide
