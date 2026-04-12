//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Manages compiler interaction: compiling, running, error parsing.
class CompilerManager final : public wxEvtHandler {
public:
    explicit CompilerManager(Context& ctx);

    /// Compile the active document. Returns true on success.
    auto compile() -> bool;

    /// Compile and run the active document.
    void compileAndRun();

    /// Run the previously compiled executable.
    void run();

    /// Compile active document as temp file, run, then clean up.
    void quickRun();

    /// Open a command prompt / terminal.
    void openCmdPrompt();

    /// Show parameters dialog for runtime arguments.
    void showParametersDialog();

    /// Toggle ShowExitCode setting.
    void toggleShowExitCode();

    /// Toggle ActivePath setting.
    void toggleActivePath();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Navigate to an error in the console list.
    void goToError(int line, const wxString& fileName);

    /// Whether a process is currently running.
    [[nodiscard]] auto isRunning() const -> bool { return m_processRunning; }

private:
    /// Ensure active document is saved before compiling. Returns false if cancelled.
    auto ensureSaved() -> bool;

    /// Build the compiler command line from the template.
    [[nodiscard]] auto buildCompileCommand() const -> wxString;

    /// Build the run command line from the template.
    [[nodiscard]] auto buildRunCommand(const wxString& exePath) const -> wxString;

    /// Parse compiler output lines and populate the console.
    void parseCompilerOutput(const wxArrayString& output);

    /// Run an executable asynchronously.
    void runAsync(const wxString& command);

    /// Called when async process terminates.
    void onProcessTerminated(int exitCode);

    Context& m_ctx;
    bool m_processRunning = false;
    bool m_isTemp = false;
    wxString m_parameters;
    wxString m_tempFolder;
    wxArrayString m_compilerOutput;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
