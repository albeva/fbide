//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/ipc.h>
#include <wx/snglinst.h>

namespace fbide {
class Context;

/// Ensures only one instance of FBIde runs at a time.
/// If another instance is already running, sends file paths
/// to it via IPC and exits. Otherwise, starts an IPC server
/// to receive file paths from future launch attempts.
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

    Context& m_ctx;
    wxSingleInstanceChecker m_checker;
    std::unique_ptr<wxServer> m_server;
};

} // namespace fbide
