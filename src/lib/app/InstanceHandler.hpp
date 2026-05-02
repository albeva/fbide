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

/**
 * Single-instance gate. On startup the handler checks whether another
 * FBIde is already running; if so it forwards the CLI file list over
 * IPC and the new process exits. Otherwise it starts an IPC server
 * so future launches can forward to *this* process.
 *
 * **Owned by:** `App` (not `Context`) — conditional state.
 *   Skipped entirely when the user passes `--new-window`.
 * **Threading:** UI thread only; IPC callbacks are dispatched onto
 * the UI thread by wx.
 */
class InstanceHandler final {
public:
    NO_COPY_AND_MOVE(InstanceHandler)

    /// Construct the handler. Starts the IPC server if no other instance is running.
    explicit InstanceHandler(Context& ctx);
    ~InstanceHandler();

    /// Check if another FBIde instance is already running.
    [[nodiscard]] auto isAnotherRunning() const -> bool;

    /// Send file paths to the running instance via IPC.
    /// If files is empty, just raises the existing instance window.
    void sendFiles(const wxArrayString& files) const;

    /// Get the IPC service name (absolute socket path on Unix).
    [[nodiscard]] static auto getServicePath() -> wxString;

private:
    Context& m_ctx;                      ///< Application context.
    wxSingleInstanceChecker m_checker;   ///< Lock used to detect prior running instances.
    std::unique_ptr<wxServer> m_server;  ///< IPC server when this is the canonical instance.
};

} // namespace fbide
