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

    /// Completion callback — invoked exactly once when the reply finishes
    /// or fails. Always on the UI thread. On a streamed success the text
    /// has already been delivered through the `ChunkHandler`, so
    /// `AiResponse::text` is left empty.
    using ResponseHandler = std::function<void(AiResponse)>;

    AiProvider() = default;
    virtual ~AiProvider() = default;

    /// Send `request`. `onChunk` receives reply text incrementally;
    /// `onComplete` runs exactly once at the end (success or error).
    /// Implementations reject overlapping calls with an error response.
    virtual void send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) = 0;

    /// Drop any per-conversation state the backend carries (resume ids,
    /// session tokens, cached system prompts). Called by `AiManager`
    /// when the conversation is cleared so the next message starts a
    /// fresh exchange on the backend's side. Default is no-op — most
    /// providers carry no per-conversation state.
    virtual void resetSession() {}
};

} // namespace fbide::ai
