//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ai/AiTypes.hpp"

namespace fbide::ai {

/**
 * Abstract AI model backend.
 *
 * Concrete providers (Anthropic, and later OpenAI / Gemini / Ollama) map
 * the provider-neutral `AiRequest` onto their wire format and the reply
 * back onto an `AiResponse`. Keeping this interface vendor-neutral is what
 * lets new backends drop in without touching `AiManager` or the UI.
 *
 * **Threading:** UI thread only. `send` is asynchronous — the handler is
 * invoked later on the UI thread.
 */
class AiProvider {
public:
    NO_COPY_AND_MOVE(AiProvider)

    /// Streaming callback — invoked 0+ times with reply text deltas as
    /// they arrive. Always on the UI thread.
    using ChunkHandler = std::function<void(const wxString& delta)>;

    /// Tool-call callback — invoked once per fully-assembled `tool_use`
    /// block from the model. The host (typically `AiManager`) collects
    /// these and dispatches them after the response completes. Always
    /// on the UI thread. Providers that don't support tools never
    /// invoke this handler.
    using ToolCallHandler = std::function<void(AiToolCall)>;

    /// Completion callback — invoked exactly once when the reply finishes
    /// or fails. Always on the UI thread. On a streamed success the text
    /// has already been delivered through the `ChunkHandler`, so
    /// `AiResponse::text` is left empty.
    using ResponseHandler = std::function<void(AiResponse)>;

    AiProvider() = default;
    virtual ~AiProvider() = default;

    /// Send `request`. `onChunk` receives reply text incrementally;
    /// `onToolCall` is fired once per assembled `tool_use` block (no-op
    /// on providers without tool support); `onComplete` runs exactly
    /// once at the end (success or error). Implementations reject
    /// overlapping calls with an error response.
    virtual void send(const AiRequest& request, ChunkHandler onChunk, ToolCallHandler onToolCall, ResponseHandler onComplete) = 0;

    /// Drop any per-conversation state the backend carries (resume ids,
    /// session tokens, cached system prompts). Called by `AiManager`
    /// when the conversation is cleared so the next message starts a
    /// fresh exchange on the backend's side. Default is no-op — most
    /// providers carry no per-conversation state.
    virtual void resetSession() {}

    /// Abort the in-flight request, if any. Implementations should
    /// eventually fire the `ResponseHandler` once with `ok = false`
    /// and a `"cancelled"` error so the host's accounting (history,
    /// dispatch loop, UI) unwinds through the normal completion path.
    /// Default no-op — providers that do not yet support cancellation
    /// (or have no in-flight state to cancel) inherit this.
    virtual void cancel() {}

    /// True when the backend attaches cache breakpoints to cacheable
    /// system blocks so a follow-up turn reuses the cached prefix at
    /// reduced cost. Default false — providers without caching collapse
    /// the structured system to a flat string via `joinSystem` and
    /// re-bill the full prompt on every turn.
    [[nodiscard]] virtual auto supportsPromptCaching() const -> bool { return false; }

    /// True when the backend can serialise `AiRequest::tools` and parse
    /// `tool_use` blocks back from the response. Default false — when
    /// false, `AiManager` skips the tool array and never sees
    /// `AiToolCall` events from this provider.
    [[nodiscard]] virtual auto supportsTools() const -> bool { return false; }
};

} // namespace fbide::ai
