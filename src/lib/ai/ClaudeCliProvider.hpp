//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiProvider.hpp"

namespace fbide {
struct ProcessResult;

/**
 * AI provider backed by the Claude Code CLI (`claude -p`).
 *
 * Spawns the `claude` binary in headless print mode instead of calling
 * an HTTP API. Authentication is whatever Claude Code is logged in as
 * (e.g. a Claude Max subscription) — no API key, no separate billing.
 *
 * The prompt is delivered on the child's stdin (not argv) so large
 * messages are not bounded by the command-line length limit.
 *
 * Conversation continuity uses `--resume <session-id>`: the first turn
 * starts a fresh session and captures its id; later turns resume it and
 * send only the newest message.
 *
 * **Threading:** UI thread only.
 */
class ClaudeCliProvider final : public AiProvider {
public:
    NO_COPY_AND_MOVE(ClaudeCliProvider)

    /// Construct with the path to the `claude` executable (just `claude`
    /// if it is on PATH).
    explicit ClaudeCliProvider(wxString claudePath);

    /// Send `request` via the Claude CLI. See `AiProvider::send`.
    void send(const AiRequest& request, ResponseHandler handler) override;

private:
    /// Turn a finished CLI invocation into an `AiResponse`, capturing the
    /// session id for the next `--resume`.
    auto handleResult(const ProcessResult& result) -> AiResponse;

    wxString m_claudePath; ///< Path to the `claude` executable.
    wxString m_sessionId;  ///< Resume id for the running conversation.
    bool m_busy = false;   ///< True while a CLI invocation is in flight.
};

} // namespace fbide
