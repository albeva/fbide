//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiProvider.hpp"
#include "AiTypes.hpp"

namespace fbide {
class Context;

/**
 * Owns the active AI provider and the running conversation.
 *
 * Reads the provider configuration from `[ai]` in the preferences on
 * construction; a provider is created only when an API key is present.
 *
 * **Owns:** the `AiProvider` and the message history.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 */
class AiManager final {
public:
    NO_COPY_AND_MOVE(AiManager)

    /// Build from config — creates a provider when an API key is present.
    explicit AiManager(Context& ctx);
    ~AiManager();

    /// True once a provider is configured (API key present).
    [[nodiscard]] auto isReady() const -> bool { return m_provider != nullptr; }

    /// Append `text` as a user message and send the whole conversation to
    /// the model. `onChunk` receives the reply incrementally; `onComplete`
    /// runs once at the end. A successful reply is appended to the history
    /// before `onComplete` runs.
    void sendMessage(const wxString& text, AiProvider::ChunkHandler onChunk, AiProvider::ResponseHandler onComplete);

    /// The conversation so far, oldest message first.
    [[nodiscard]] auto history() const -> const std::vector<AiMessage>& { return m_history; }

    /// Drop the conversation history.
    void clear() { m_history.clear(); }

private:
    Context& m_ctx;                         ///< Application context.
    std::unique_ptr<AiProvider> m_provider; ///< Active backend (null until configured).
    std::vector<AiMessage> m_history;       ///< Conversation messages.
    wxString m_model;                       ///< Model name sent with each request.
};

} // namespace fbide
