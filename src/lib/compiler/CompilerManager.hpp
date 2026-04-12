//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Document;
class CompileCommand;
class Context;

/// Result of executing the compiler.
struct CompileResult {
    int exitCode = -1;
    wxArrayString output;
    wxString compiledFile;
};

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
    void openCmdPrompt(); // TODO: does not belong in CompilerManager

    /// Show parameters dialog for runtime arguments.
    void showParametersDialog(); // TODO: does not belong in CompilerManager

    /// Toggle ShowExitCode setting.
    void toggleShowExitCode();

    /// Toggle ActivePath setting.
    void toggleActivePath();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

    /// Whether a process is currently running.
    [[nodiscard]] auto isRunning() const -> bool { return m_processRunning; }

    /// Called when async process terminates.
    void onProcessTerminated(int exitCode);

private:
    struct CompileOptions final {
        // compile as temporary file FBIDETEMP.BAS
        bool temporary;
    };

    /// Compile given document, will ask to save
    auto compile(Document* doc, CompileOptions options) -> std::optional<CompileResult>;

    // -- Compile pipeline steps --

    /// Ensure active document is saved before compiling. Returns false if cancelled.
    auto prepareDocument() -> bool;

    /// Build a CompileCommand from Config settings and the given source file.
    [[nodiscard]] auto buildCompileCommand(const wxString& sourceFile) const -> CompileCommand;

    /// Execute a CompileCommand synchronously and return the result.
    [[nodiscard]] auto executeCompiler(const CompileCommand& cmd) -> CompileResult;

    /// Process compiler output: parse errors, update console and log.
    /// Returns true if errors were found.
    auto processResult(const CompileResult& result) -> bool;

    // -- Helpers --

    /// Build the run command line from the config template.
    [[nodiscard]] auto buildRunCommand(const wxFileName& file) const -> wxString;

    /// Parse compiler output lines and populate the console.
    /// Returns true if errors were found.
    auto parseCompilerOutput(const wxArrayString& output) -> bool;

    /// Run an executable asynchronously.
    void runAsync(const wxString& command);

    /// Clean up temp files after quick run.
    void cleanupTempFiles();

    /// Resolve and validate the fbc compiler path. Returns empty on failure.
    [[nodiscard]] auto resolveCompilerPath() const -> wxString;

    Context& m_ctx;
    bool m_processRunning = false;
    bool m_isTemp = false;
    wxString m_parameters;
    wxString m_tempFolder;
    wxArrayString m_compilerLog;
    wxString m_firstErrorFile;
    int m_firstErrorLine = -1;
};

} // namespace fbide
