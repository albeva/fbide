//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ClaudeCliProvider.hpp"
#include <nlohmann/json.hpp>
#include "compiler/AsyncProcess.hpp"
#include "compiler/QuoteUtils.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {
constexpr int kStderrSnippetLength = 300;
} // namespace

ClaudeCliProvider::ClaudeCliProvider(wxString claudePath)
: m_claudePath(std::move(claudePath)) {}

void ClaudeCliProvider::send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }
    if (request.messages.empty()) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "Nothing to send." });
        return;
    }

    // A conversation back to a single message is a fresh start — drop any
    // stale resume id (e.g. after the history was cleared).
    if (request.messages.size() <= 1) {
        m_sessionId.clear();
    }

    // --resume keeps the prior turns in the CLI session, so only the
    // newest message needs to be sent. It goes on stdin, not argv, so its
    // length is not bounded by the command-line limit.
    const wxString& prompt = request.messages.back().content;

    wxString command = quoteArg(m_claudePath);
    command += " -p --output-format stream-json --verbose --include-partial-messages";
    if (!request.model.empty()) {
        command += " --model " + quoteArg(request.model);
    }
    if (!m_sessionId.empty()) {
        command += " --resume " + quoteArg(m_sessionId);
    }
    if (!request.system.empty()) {
        command += " --append-system-prompt " + quoteArg(request.system);
    }

    // Reset the per-request state collected from the stream events.
    m_sawResult = false;
    m_isError = false;
    m_resultText.clear();
    m_pendingSessionId.clear();
    m_busy = true;

    AsyncProcess::exec(
        command, {}, /*redirect=*/true,
        [this, onComplete = std::move(onComplete)](ProcessResult result) {
            m_busy = false;
            onComplete(buildResponse(result));
        },
        prompt,
        [this, onChunk = std::move(onChunk)](const wxString& line) {
            handleLine(line, onChunk);
        }
    );
}

void ClaudeCliProvider::handleLine(const wxString& line, const ChunkHandler& onChunk) {
    const auto event = json::parse(line.utf8_string(), nullptr, false);
    if (event.is_discarded()) {
        return;
    }

    const auto type = event.value("type", "");
    if (type == "stream_event") {
        // Partial-message event — the actual token deltas.
        const auto& inner = event["event"];
        if (inner.is_object() && inner.value("type", "") == "content_block_delta") {
            const auto& delta = inner["delta"];
            if (delta.is_object() && delta.value("type", "") == "text_delta") {
                onChunk(wxString::FromUTF8(delta.value("text", "")));
            }
        }
    } else if (type == "result") {
        // Terminal event — carries the final status and the resume id.
        m_sawResult = true;
        m_isError = event.value("is_error", false);
        m_resultText = wxString::FromUTF8(event.value("result", ""));
        m_pendingSessionId = wxString::FromUTF8(event.value("session_id", ""));
    }
}

auto ClaudeCliProvider::buildResponse(const ProcessResult& result) -> AiResponse {
    AiResponse response;

    if (!result.launched) {
        response.error = "Could not launch the Claude CLI. Make sure Claude Code is installed; "
                         "set [ai] claudePath to the full path to the executable.";
        return response;
    }

    if (!m_sawResult) {
        wxString stderrText;
        for (const auto& line : result.output) {
            stderrText += line;
            stderrText += '\n';
        }
        stderrText = stderrText.Strip(wxString::both);
        response.error = "The Claude CLI produced no result.";
        if (!stderrText.empty()) {
            response.error += " " + stderrText.Left(kStderrSnippetLength);
        }
        return response;
    }

    if (m_isError || result.exitCode != 0) {
        response.error = m_resultText.empty() ? wxString("The Claude CLI reported an error.") : m_resultText;
        return response;
    }

    m_sessionId = m_pendingSessionId;
    response.ok = true;
    // Fallback for a CLI build that streams no partial deltas — AiManager
    // prefers the streamed text and uses this only when nothing streamed.
    response.text = m_resultText;
    return response;
}
