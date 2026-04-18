//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AsyncProcess.hpp"

namespace fbide {
class Context;
class Document;

/// Handles the compile-and-run lifecycle for a single document.
///
/// All compilation and execution is asynchronous via AsyncProcess.
/// Created by CompilerManager for each compile/run operation.
class BuildTask final {
public:
    NO_COPY_AND_MOVE(BuildTask)

    static constexpr auto TEMPNAME = "FBIDETEMP.BAS";

    /// Create a compile process.
    /// @param ctx Application context.
    /// @param doc The document being compiled, or nullptr.
    BuildTask(Context& ctx, Document* doc);

    /// Compile the given source file asynchronously.
    void compile(const wxString& sourceFile);

    /// Compile the source file and run the resulting executable.
    void compileAndRun(const wxString& sourceFile, bool quickRun);

    /// Run a previously compiled executable.
    void run(const wxString& executablePath, bool quickRun);

    /// Whether an async process is currently running.
    [[nodiscard]] auto isRunning() const -> bool { return m_running; }

    /// Is this a quickrun task?
    [[nodiscard]] auto isQuickRun() const -> bool { return m_isQuickRun; }

    /// Get the full compiler log for display.
    [[nodiscard]] auto getCompilerLog() const -> const wxArrayString& { return m_compilerLog; }

    /// Get the compiled executable path from the last successful compile.
    [[nodiscard]] auto getCompiledFile() const -> const wxString& { return m_compiledFile; }

    /// Get the document if still valid, or nullptr.
    [[nodiscard]] auto getDocument() const -> Document*;

private:
    /// Start the compiler asynchronously.
    void startCompiler(const wxString& sourceFile);

    /// Handle compiler process completion.
    void onCompileFinished(const ProcessResult& result);

    /// Handle run process completion.
    void onRunFinished(const ProcessResult& result);

    /// Parse compiler output, populate console. Returns true if errors found.
    auto showErrors(const wxArrayString& output) -> bool;

    /// Derive the executable path from a source file path.
    [[nodiscard]] static auto deriveExecutablePath(const wxString& sourceFile) -> wxString;

    /// Build the run command from config template and executable path.
    [[nodiscard]] auto buildRunCommand(const wxString& executablePath) const -> wxString;

    /// Append system info (FBIde version, fbc version, OS) to the compiler log.
    void appendSystemInfo();

    /// Clean up temp files from quick run.
    void cleanupTempFiles();

    /// Set status bar text from a locale path (empty for none).
    void setStatus(const wxString& path) const;

    Context& m_ctx;
    Document* m_doc;
    bool m_running = false;
    bool m_shouldRun = false;
    bool m_isQuickRun = false;
    wxString m_sourceFile;
    wxString m_buildDir;
    wxString m_compiledFile;
    wxArrayString m_compilerLog;
};

} // namespace fbide
