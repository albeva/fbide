//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AsyncProcess.hpp"
#include "CompilerConfigCatalog.hpp"

namespace fbide {
class Context;
class Document;
class CompilerLog;
enum class CompilerLogSection : std::size_t;

/// Handles the compile-and-run lifecycle for a single document.
///
/// All compilation and execution is asynchronous via AsyncProcess.
/// Created by CompilerManager for each compile/run operation.
class BuildTask final {
public:
    NO_COPY_AND_MOVE(BuildTask)

    /// Canonical filename for the QuickRun temp source.
    static constexpr auto TEMPNAME = "FBIDETEMP.BAS";

    /// Create a compile process.
    /// @param ctx Application context.
    /// @param doc The document being compiled, or nullptr.
    BuildTask(Context& ctx, Document* doc);

    /// Sever any in-flight process's callback before teardown — the callback
    /// captures `this`, so a late OnTerminate after destruction would be a
    /// use-after-free. Does not kill the child (shutdown does that explicitly
    /// via `CompilerManager::killProcess`).
    ~BuildTask();

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

    /// Get the compiled executable path from the last successful compile.
    [[nodiscard]] auto getCompiledFile() const -> const wxString& { return m_compiledFile; }

    /// Get the document if still valid, or nullptr.
    [[nodiscard]] auto getDocument() const -> Document*;

    /// Kill this task
    void kill();

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

    /// Set the system-info section (FBIde version, fbc version, OS).
    void appendSystemInfo();

    /// The compiler-log window — owns the section content and rendering.
    [[nodiscard]] auto compilerLog() const -> CompilerLog&;

    /// Clean up temp files from quick run.
    void cleanupTempFiles();

    /// Set status bar text from a locale path (empty for none).
    void setStatus(const wxString& path) const;

    Context& m_ctx;                    ///< Application context.
    Document* m_doc;                   ///< Document this task is bound to (nullable).
    ResolvedCompilerConfig m_config;   ///< Snapshot captured at construction — stable across compile + run.
    bool m_running = false;            ///< True while a process is in flight.
    bool m_shouldRun = false;          ///< True when a successful compile should chain into run.
    bool m_isQuickRun = false;         ///< True for QuickRun (compile to temp file + run).
    wxString m_sourceFile;             ///< Source file currently being compiled.
    wxString m_buildDir;               ///< Working directory for the compile/run process.
    wxString m_compiledFile;           ///< Path of the produced executable (set on success).
    wxString m_fbcVersion;             ///< Active config's fbc version, probed before the async compile.
    AsyncProcess* m_process = nullptr; ///< In-flight async process (self-deleting).
};

} // namespace fbide
