//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <string>

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
/// Optionally streams stdout line-by-line through a `LineHandler` as the
/// data arrives (used by the AI Claude-CLI provider). In streaming mode
/// stdout is delivered through that handler and not collected into
/// `ProcessResult::output`; stderr is still batched there.
///
/// Self-managing lifetime — caller must allocate with new,
/// the process deletes itself after invoking the callback.
class AsyncProcess final : wxProcess {
public:
    NO_COPY_AND_MOVE(AsyncProcess)

    /// Termination callback signature.
    using Callback = std::function<void(ProcessResult)>;

    /// Per-line stdout streaming callback, invoked as complete lines
    /// arrive (before the termination callback).
    using LineHandler = std::function<void(const wxString&)>;

    /// Launch the command asynchronously.
    /// @param command    The command line to execute.
    /// @param workingDir Working directory for the process. Empty = inherit.
    /// @param redirect   If true, capture stdout/stderr into ProcessResult::output.
    /// @param callback Called exactly once when the process terminates (or fails to launch).
    /// @param input    Optional UTF-8 data written to the child's stdin, then
    ///                 closed. Requires `redirect` — ignored otherwise.
    /// @param onLine   Optional stdout line callback. When set, stdout is
    ///                 streamed line-by-line instead of batched. Requires
    ///                 `redirect`.
    static auto exec(
        const wxString& command,
        const wxString& workingDir,
        bool redirect,
        Callback&& callback,
        const wxString& input = {},
        LineHandler onLine = {}
    ) -> AsyncProcess*;

    /// Kill the process
    void kill();

private:
    /// Create an async process.
    /// @param callback Called exactly once when the process terminates (or fails to launch).
    explicit AsyncProcess(Callback&& callback);

    /// Launch the command asynchronously.
    /// @param command    The command line to execute.
    /// @param workingDir Working directory for the process. Empty = inherit.
    /// @param redirect   If true, capture stdout/stderr into ProcessResult::output.
    /// @param input      UTF-8 data to write to the child's stdin (then close).
    /// @param onLine     Optional stdout line streaming callback.
    void exec(
        const wxString& command,
        const wxString& workingDir,
        bool redirect,
        const wxString& input,
        LineHandler onLine
    );

    /// wxProcess hook — invoked when the child process exits. Calls `m_callback`
    /// then deletes `this`.
    // ReSharper disable once CppOverrideWithDifferentVisibility
    void OnTerminate(int pid, int status) override;

    /// Poll timer tick — drains whatever stdout is currently available.
    void onPollTimer(wxTimerEvent& event);

    /// Read whatever stdout data is available without blocking, splitting
    /// it into lines emitted through `m_onLine`. When `flush` is true, any
    /// trailing partial line is emitted as well.
    void drainLines(bool flush);

    /// Drain `stream` line-by-line into `output`.
    static void readStream(wxInputStream* stream, wxArrayString& output);

    Callback m_callback;      ///< User-supplied termination callback.
    LineHandler m_onLine;     ///< Streaming stdout line callback (empty = batch mode).
    std::string m_lineBuffer; ///< Stdout bytes not yet split into a complete line.
    wxTimer m_pollTimer;      ///< Drives stdout polling while streaming.
};

} // namespace fbide
