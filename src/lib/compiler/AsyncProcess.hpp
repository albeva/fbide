//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Result of an async process execution.
struct ProcessResult final {
    int exitCode = -1;    ///< Process exit code (`-1` when launch failed).
    wxArrayString output; ///< Captured stdout/stderr lines (when redirected).

    /// True if the process was launched successfully and terminated.
    /// False if wxExecute failed to start the process.
    bool launched = false;

    /// Convenience: true if launched and exited with code 0.
    [[nodiscard]] explicit operator bool() const { return launched && exitCode == EXIT_SUCCESS; }
};

/// Reusable async process wrapper.
///
/// Launches a command asynchronously, optionally captures stdout/stderr,
/// and invokes a callback with the result. Always calls the callback
/// exactly once, even if the process fails to launch.
///
/// Self-managing lifetime — caller must allocate with new,
/// the process deletes itself after invoking the callback.
class AsyncProcess final : wxProcess {
public:
    NO_COPY_AND_MOVE(AsyncProcess)

    /// Termination callback signature.
    using Callback = std::function<void(ProcessResult)>;

    /// Launch the command asynchronously.
    /// @param command    The command line to execute.
    /// @param workingDir Working directory for the process. Empty = inherit.
    /// @param redirect   If true, capture stdout/stderr into ProcessResult::output.
    /// @param callback Called exactly once when the process terminates (or fails to launch).
    /// @return The running process, or nullptr if it failed to launch (in
    ///         which case the callback has already fired and the object is
    ///         gone — nothing to store).
    static auto exec(
        const wxString& command,
        const wxString& workingDir,
        bool redirect,
        Callback&& callback
    ) -> AsyncProcess*;

    /// Kill the process
    void kill();

    /// Sever the termination callback. Call this when the callback's owner
    /// is about to be destroyed while the process may still be running, so a
    /// late OnTerminate can't dereference freed memory. The object still
    /// self-deletes when the process eventually terminates.
    void detach();

private:
    /// Create an async process.
    /// @param callback Called exactly once when the process terminates (or fails to launch).
    explicit AsyncProcess(Callback&& callback);

    /// Launch the command asynchronously.
    /// @param command    The command line to execute.
    /// @param workingDir Working directory for the process. Empty = inherit.
    /// @param redirect   If true, capture stdout/stderr into ProcessResult::output.
    /// @return true if launched; false if launch failed (in which case the
    ///         callback has fired and the object has deleted itself).
    auto exec(
        const wxString& command,
        const wxString& workingDir,
        bool redirect
    ) -> bool;

    /// wxProcess hook — invoked when the child process exits. Calls `m_callback`
    /// then deletes `this`.
    // ReSharper disable once CppOverrideWithDifferentVisibility
    void OnTerminate(int pid, int status) override;

    /// Drain `stream` line-by-line into `output`.
    static void readStream(wxInputStream* stream, wxArrayString& output);

    Callback m_callback; ///< User-supplied termination callback.
};

} // namespace fbide
