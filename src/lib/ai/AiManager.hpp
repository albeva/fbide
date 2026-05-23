//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiContext.hpp"
#include "AiTypes.hpp"
#include "provider/AiProvider.hpp"

namespace fbide {
class Context;
} // namespace fbide

namespace fbide::ai {

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
    ~AiManager() = default;

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

    /// The set of files/items attached to the conversation as context.
    [[nodiscard]] auto context() -> AiContext& { return m_context; }

    /// Agent mode — when true, the system prompt is rewritten to request
    /// edits as SEARCH/REPLACE blocks against the pinned edit target.
    /// Off (default) keeps the historical chat behaviour.
    [[nodiscard]] auto isAgentMode() const -> bool { return m_agentMode; }
    void setAgentMode(const bool on) { m_agentMode = on; }

    /// Live-edit — when true and the bubble carries a complete patch
    /// proposal, the chat applies it as it streams. Off (default) keeps
    /// proposals manual.
    [[nodiscard]] auto isLiveEdit() const -> bool { return m_liveEdit; }
    void setLiveEdit(const bool on) { m_liveEdit = on; }

private:
    Context& m_ctx;                         ///< Application context.
    std::unique_ptr<AiProvider> m_provider; ///< Active backend (null until configured).
    std::vector<AiMessage> m_history;       ///< Conversation messages.
    AiContext m_context;                    ///< Files attached as context.
    wxString m_model;                       ///< Model name sent with each request.
    wxString m_systemPrompt;                ///< Configured system prompt (may be empty).
    bool m_agentMode = false;               ///< Agent mode toggle state.
    bool m_liveEdit = false;                ///< Live-edit auto-apply toggle state.
};

} // namespace fbide::ai
