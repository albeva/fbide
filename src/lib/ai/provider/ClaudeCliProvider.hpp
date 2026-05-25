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
} // namespace fbide

namespace fbide::ai {

/**
 * AI provider backed by the Claude Code CLI (`claude -p`).
 *
 * Spawns the `claude` binary in headless print mode instead of calling
 * an HTTP API. Authentication is whatever Claude Code is logged in as
 * (e.g. a Claude Max subscription) — no API key, no separate billing.
 *
 * The prompt is delivered on the child's stdin (not argv) so large
 * messages are not bounded by the command-line length limit. The reply
 * is streamed: `--output-format stream-json --include-partial-messages`
 * emits JSON events line-by-line, parsed into text deltas.
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

    /// Send `request` via the Claude CLI. See `AiProvider::send`. The
    /// tool-call handler is ignored — Claude CLI's stream-json output
    /// does not expose tool_use events at this layer.
    void send(const AiRequest& request, ChunkHandler onChunk, ToolCallHandler onToolCall, ResponseHandler onComplete) override;

    /// Drop the active `--resume` session id so the next `send` starts
    /// a fresh CLI conversation. Called from `AiManager::clear`.
    void resetSession() override { m_sessionId.clear(); }

private:
    /// Handle one stdout line (a JSON stream event) from the CLI.
    /// Emits text deltas through `m_onChunk` when the line carries one.
    void handleLine(const wxString& line);

    /// Build the final response once the process has exited.
    auto buildResponse(const ProcessResult& result) -> AiResponse;

    wxString m_claudePath;        ///< Path to the `claude` executable.
    wxString m_sessionId;         ///< Resume id for the running conversation.
    wxString m_pendingSessionId;  ///< Session id from the in-flight request.
    wxString m_resultText;        ///< `result` text of the in-flight request.
    ChunkHandler m_onChunk;       ///< Streaming delta callback for the in-flight request.
    ResponseHandler m_onComplete; ///< Pending completion callback.
    int m_inputTokens = 0;        ///< Input tokens reported on the `result` event.
    int m_outputTokens = 0;       ///< Output tokens reported on the `result` event.
    bool m_sawResult = false;     ///< A `result` event was seen this request.
    bool m_isError = false;       ///< The `result` event reported an error.
    bool m_busy = false;          ///< True while a CLI invocation is in flight.
};

} // namespace fbide::ai
