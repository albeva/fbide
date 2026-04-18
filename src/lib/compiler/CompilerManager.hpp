//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/LangId.hpp"

namespace fbide {
class BuildTask;
class Document;
class Context;

/// Manages compiler interaction: validates documents, resolves
/// the compiler, and delegates to BuildTask for execution.
class CompilerManager final {
public:
    NO_COPY_AND_MOVE(CompilerManager)

    explicit CompilerManager(Context& ctx);
    ~CompilerManager();

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

    /// Refresh the compiler log dialog if it exists.
    void refreshCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

    /// Get the fbc version string. Validates the compiler path, runs `fbc --version`,
    /// and caches the result. Returns empty string if compiler is not accessible.
    [[nodiscard]] auto getFbcVersion() -> const wxString&;

    /// Reset the cached fbc version. Call when compiler path may have changed.
    void resetFbcVersion() { m_fbcVersion.clear(); }

    /// Get runtime parameters for the executable.
    [[nodiscard]] auto getParameters() const -> const wxString& { return m_parameters; }

    /// Set runtime parameters (from the parameters dialog).
    void setParameters(const wxString& params) { m_parameters = params; }

private:
    /// Get active FreeBASIC document, or nullptr if unavailable.
    [[nodiscard]] auto getActiveDocument() -> Document*;

    /// Ensure document is saved. Returns false if user cancelled.
    auto ensureSaved(Document& doc) -> bool;

    /// Set status bar text.
    void setStatus(LangId id) const;

    Context& m_ctx;
    std::unique_ptr<BuildTask> m_task;
    wxString m_parameters;
    wxString m_fbcVersion;
};

} // namespace fbide
