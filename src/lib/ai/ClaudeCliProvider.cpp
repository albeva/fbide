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
using json = nlohmann::json;

ClaudeCliProvider::ClaudeCliProvider(wxString claudePath)
: m_claudePath(std::move(claudePath)) {}

void ClaudeCliProvider::send(const AiRequest& request, ResponseHandler handler) {
    if (m_busy) {
        handler(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }
    if (request.messages.empty()) {
        handler(AiResponse { .ok = false, .text = {}, .error = "Nothing to send." });
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
    command += " -p --output-format json";
    if (!request.model.empty()) {
        command += " --model " + quoteArg(request.model);
    }
    if (!m_sessionId.empty()) {
        command += " --resume " + quoteArg(m_sessionId);
    }
    if (!request.system.empty()) {
        command += " --append-system-prompt " + quoteArg(request.system);
    }

    m_busy = true;
    AsyncProcess::exec(
        command, {}, /*redirect=*/true,
        [this, handler = std::move(handler)](ProcessResult result) {
            m_busy = false;
            handler(handleResult(result));
        },
        prompt
    );
}

auto ClaudeCliProvider::handleResult(const ProcessResult& result) -> AiResponse {
    AiResponse response;

    if (!result.launched) {
        response.error = "Could not launch the Claude CLI. Make sure Claude Code is installed; "
                         "set [ai] claudePath to the full path to the executable.";
        return response;
    }

    // stdout and stderr are merged into result.output — the JSON result
    // object is somewhere in that blob.
    wxString blob;
    for (const auto& line : result.output) {
        blob += line;
        blob += '\n';
    }

    const auto parsed = json::parse(blob.utf8_string(), nullptr, false);
    if (parsed.is_discarded()) {
        response.error = "Could not parse the Claude CLI output: " + blob.Left(200);
        return response;
    }

    // Capture the session id so the next turn can --resume it.
    if (const auto session = parsed.value("session_id", ""); !session.empty()) {
        m_sessionId = wxString::FromUTF8(session);
    }

    const auto text = wxString::FromUTF8(parsed.value("result", ""));
    if (parsed.value("is_error", false) || result.exitCode != 0) {
        response.error = text.empty() ? wxString("The Claude CLI reported an error.") : text;
        return response;
    }

    response.text = text;
    response.ok = true;
    return response;
}
