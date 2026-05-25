//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ToolDispatchLoop.hpp"
using namespace fbide;
using namespace fbide::ai;

ToolDispatchLoop::ToolDispatchLoop(AiProvider& provider, ToolInvoker invokeTool)
: m_provider(provider)
, m_invokeTool(std::move(invokeTool)) {}

void ToolDispatchLoop::run(
    std::vector<AiMessage>* history,
    RequestFactory requestFactory,
    AiProvider::ChunkHandler onChunk,
    FinishHandler onDone
) {
    wxASSERT_MSG(!m_running, "ToolDispatchLoop::run called while another run is in flight");
    m_history = history;
    m_requestFactory = std::move(requestFactory);
    m_onChunk = std::move(onChunk);
    m_onDone = std::move(onDone);
    m_round = 0;
    m_running = true;
    m_accumulator.clear();
    m_pendingCalls.clear();
    m_pendingResults.clear();
    m_nextToolIndex = 0;
    sendNextTurn();
}

void ToolDispatchLoop::sendNextTurn() {
    m_accumulator.clear();
    m_pendingCalls.clear();
    const auto request = m_requestFactory();
    m_provider.send(
        request,
        [this](const wxString& delta) {
            m_accumulator += delta;
            if (m_onChunk) {
                m_onChunk(delta);
            }
        },
        [this](AiToolCall call) { m_pendingCalls.push_back(std::move(call)); },
        [this](AiResponse response) { onTurnComplete(std::move(response)); }
    );
}

void ToolDispatchLoop::onTurnComplete(AiResponse response) {
    if (!response.ok) {
        // Provider error — append nothing (history would be inconsistent
        // with the wire), surface the failure as-is.
        finish(std::move(response));
        return;
    }
    // Append the assistant turn. Text comes from the streamed
    // accumulator (preferred) or the response's text field (non-
    // streaming providers).
    const wxString full = m_accumulator.empty() ? response.text : m_accumulator;
    m_history->push_back({
        .role = AiRole::Assistant,
        .content = full,
        .toolCalls = m_pendingCalls,
    });

    if (m_pendingCalls.empty()) {
        // No tools requested — conversation terminates here.
        finish(std::move(response));
        return;
    }

    if (m_round + 1 >= kMaxRounds) {
        // Cap reached. Surface the failure so the user knows the
        // model didn't reach a natural stop.
        finish(AiResponse {
            .ok = false,
            .text = {},
            .error = wxString::Format("Tool dispatch cap (%d rounds) exceeded.", kMaxRounds),
        });
        return;
    }
    m_round++;

    // Invoke tools sequentially. The first call fires from here; each
    // subsequent call is launched from the previous one's result
    // handler in `invokeNextTool`. Sequential because mixed sync/async
    // tools share state via member variables — a parallel design would
    // need per-call state objects.
    m_pendingResults.clear();
    m_pendingResults.reserve(m_pendingCalls.size());
    m_nextToolIndex = 0;
    invokeNextTool();
}

void ToolDispatchLoop::invokeNextTool() {
    if (m_nextToolIndex >= m_pendingCalls.size()) {
        // All tools done — emit one user message holding the results
        // and start the next round.
        m_history->push_back({
            .role = AiRole::User,
            .content = {},
            .toolResults = std::move(m_pendingResults),
        });
        sendNextTurn();
        return;
    }
    auto call = std::move(m_pendingCalls.at(m_nextToolIndex));
    m_nextToolIndex++;
    m_invokeTool(std::move(call), [this](AiToolResult result) {
        m_pendingResults.push_back(std::move(result));
        invokeNextTool();
    });
}

void ToolDispatchLoop::cancel() {
    if (!m_running) {
        return;
    }
    // Forward to the provider — its cancellation will eventually fire
    // the response handler with an error, which routes through
    // `onTurnComplete` → `finish` like any other failed turn. No need
    // to mutate state here; double-cancel is idempotent because the
    // provider's own cancel is a no-op when idle.
    m_provider.cancel();
}

void ToolDispatchLoop::finish(AiResponse response) {
    auto onDone = std::exchange(m_onDone, nullptr);
    m_onChunk = nullptr;
    m_requestFactory = nullptr;
    m_history = nullptr;
    m_running = false;
    m_pendingCalls.clear();
    m_pendingResults.clear();
    m_accumulator.clear();
    if (onDone) {
        onDone(std::move(response));
    }
}
