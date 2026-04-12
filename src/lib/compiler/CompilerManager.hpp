//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/config/LangId.hpp"

namespace fbide {
class Document;
class Context;

/// Manages compiler interaction: compiling, running, error parsing.
class CompilerManager final : public wxEvtHandler {
public:
    explicit CompilerManager(Context& ctx);

    /// Compile the active document.
    void compile();

    /// Compile and run the active document.
    void compileAndRun();

    /// Run the previously compiled executable (or offer to compile first).
    void run();

    /// Compile active document as temp file, run, then clean up.
    void quickRun();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

    /// Called when async process terminates.
    void onProcessTerminated(int exitCode);

private:
    /// A compile job flowing through the pipeline.
    struct CompileJob final {
        Document* doc = nullptr;
        wxString sourceFile;
        wxString compiledFile;
    };

    // -- Pipeline steps --

    /// Get active FreeBASIC document, or nullptr.
    [[nodiscard]] auto getActiveDocument() -> Document*;

    /// Ensure document is saved. Returns false if user cancelled.
    auto ensureSaved(Document& doc) -> bool;

    /// Resolve and validate the fbc compiler path. Returns empty on failure.
    [[nodiscard]] auto resolveCompiler() const -> wxString;

    /// Execute the compiler on a source file. Returns exit code and output.
    auto executeCompiler(const wxString& compiler, const wxString& sourceFile) -> CompileJob*;

    /// Parse compiler output, populate console. Returns true if errors found.
    auto showErrors(const wxArrayString& output) -> bool;

    /// Derive the executable path from a source file path.
    [[nodiscard]] static auto deriveExecutablePath(const wxString& sourceFile) -> wxString;

    /// Build the run command from config template and executable path.
    [[nodiscard]] auto buildRunCommand(const wxString& executablePath) const -> wxString;

    /// Launch an executable asynchronously.
    void runAsync(const wxString& command);

    /// Clean up temp files from quick run.
    void cleanupTempFiles();

    /// Set status bar text.
    void setStatus(LangId id) const;

    Context& m_ctx;
    bool m_processRunning = false;
    bool m_cleanupOnExit = false;
    wxString m_parameters;
    wxString m_tempFolder;
    wxArrayString m_compilerLog;

    // Current compile job (valid during compile pipeline)
    std::optional<CompileJob> m_job;
};

} // namespace fbide
