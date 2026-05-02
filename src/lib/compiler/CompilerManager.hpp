//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class BuildTask;
class Document;
class Context;

/**
 * Owns FBIde's relationship with `fbc`: the compile / run / quickrun
 * commands, the compiler probe, the runtime parameters, and the
 * single in-flight `BuildTask`.
 *
 * **Owns:** the current `m_task` (`unique_ptr<BuildTask>`), cached
 * `m_fbcVersion`, and runtime `m_parameters`.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. `BuildTask` spawns `AsyncProcess`,
 * which is async with a UI-thread callback — there is no
 * compiler-side worker.
 * **Single in-flight:** replacing `m_task` deletes the previous task
 * and aborts any process it had spawned. Two builds cannot race.
 *
 * See @ref compiler.
 */
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

    /// Kill active compile or run task
    void killProcess();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Refresh the compiler log dialog if it exists.
    void refreshCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

    /// Get the fbc version string. Validates the compiler path, runs `fbc --version`,
    /// and caches the result. Returns empty string if compiler is not accessible.
    [[nodiscard]] auto getFbcVersion() -> const wxString&;

    /// Resolve `compiler.path` against the IDE's appDir and verify the
    /// binary exists/is executable. Returns the resolved absolute path,
    /// or empty when the path is unset or missing.
    [[nodiscard]] auto resolveCompilerBinary() const -> wxString;

    /// Startup probe: when the configured compiler binary is missing,
    /// surface a wxRichMessageDialog with a "Don't show again" checkbox.
    /// "Yes" opens the Settings dialog focused on the Compiler tab; the
    /// checkbox toggles `alerts.ignore.missingCompilerBinary` so future
    /// launches stay silent. No-op when the binary is reachable or the
    /// ignore flag is set. Call once after the main frame is created.
    void checkCompilerOnStartup();

    /// Show the "compiler not found" prompt without the silence checkbox
    /// (used by the build flow when the user explicitly invokes compile/
    /// run): the alert is always relevant because the user just asked for
    /// the compiler. "Yes" opens the Settings dialog on the Compiler tab.
    void promptMissingCompiler();

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

    /// Set status bar text from locale path (empty for none).
    void setStatus(const wxString& path) const;

    Context& m_ctx;
    std::unique_ptr<BuildTask> m_task;
    wxString m_parameters;
    wxString m_fbcVersion;
};

} // namespace fbide
