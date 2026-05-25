//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiTypes.hpp"
#include "provider/AiProvider.hpp"
#include "tools/ToolRegistry.hpp"

namespace fbide::ai {

/**
 * Multi-round dispatch loop that drives a conversation through any
 * tool calls the model issues before terminating.
 *
 * Each round:
 *   1. Build a fresh `AiRequest` from the current history.
 *   2. Send it through the provider, collecting streamed text and
 *      `tool_use` blocks the model emits.
 *   3. Append the assistant message (text + tool calls) to history.
 *   4. If no tool calls, fire `onDone` and stop.
 *   5. Otherwise invoke each tool, collect results, append a user
 *      message holding the tool_result blocks, re-enter.
 *
 * Caps at `kMaxRounds` rounds — a model that keeps demanding tool
 * calls gets a structured error rather than running forever. Owns the
 * per-run state so concurrent loops would collide; only one `run` may
 * be in flight at a time.
 *
 * **Threading:** UI thread only.
 */
class ToolDispatchLoop final {
public:
    NO_COPY_AND_MOVE(ToolDispatchLoop)

    /// Maximum number of provider round-trips per `run`. Bound chosen
    /// to allow agentic edit-verify cycles without runaway loops.
    static constexpr int kMaxRounds = 10;

    /// How the loop invokes a tool. Mirrors `ToolRegistry::invoke` so
    /// AiManager can pass its registry's `invoke` straight through;
    /// tests can substitute a stub invoker.
    using ToolInvoker = std::function<void(AiToolCall, Tool::ResultHandler)>;

    /// Produces the next request from the current state. Called before
    /// every send so the system prompt and tool list always reflect
    /// the latest `history`.
    using RequestFactory = std::function<AiRequest()>;

    /// Final callback — fired exactly once at termination (no more
    /// tool calls, provider error, or round-cap exceeded).
    using FinishHandler = std::function<void(AiResponse)>;

    ToolDispatchLoop(AiProvider& provider, ToolInvoker invokeTool);

    /// Drive the conversation. `history` is owned by the caller and is
    /// mutated as the loop progresses. `requestFactory` produces a
    /// fresh `AiRequest` per round; the loop does not cache it. The
    /// handlers fire on the UI thread. Asserts when called while a
    /// previous `run` is still in flight.
    void run(
        std::vector<AiMessage>* history,
        RequestFactory requestFactory,
        AiProvider::ChunkHandler onChunk,
        FinishHandler onDone
    );

    /// Streamed text accumulated for the in-flight turn — what the
    /// chat view paints as the "currently streaming" assistant bubble.
    /// Cleared at the start of each turn and at termination.
    [[nodiscard]] auto pendingReply() const -> const wxString& { return m_accumulator; }

private:
    void sendNextTurn();
    void onTurnComplete(AiResponse response);
    void invokeNextTool();
    void finish(AiResponse response);

    AiProvider& m_provider;
    ToolInvoker m_invokeTool;

    // Per-run state. Reset at the start of each `run`; cleared in
    // `finish` so a follow-up `run` sees a clean slate.
    std::vector<AiMessage>* m_history = nullptr;
    RequestFactory m_requestFactory;
    AiProvider::ChunkHandler m_onChunk;
    FinishHandler m_onDone;
    int m_round = 0;
    bool m_running = false;
    wxString m_accumulator;
    std::vector<AiToolCall> m_pendingCalls;
    std::vector<AiToolResult> m_pendingResults;
    std::size_t m_nextToolIndex = 0;
};

} // namespace fbide::ai
