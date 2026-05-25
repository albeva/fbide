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

    /// Partial reply text accumulated for the in-flight request. Empty
    /// between requests and after `clear()`. The chat panel reads this
    /// to render the streaming bubble — having the manager be the
    /// single owner of the accumulator avoids a second growing
    /// wxString duplicate on the panel side.
    [[nodiscard]] auto pendingReply() const -> const wxString& { return m_pendingAccumulator; }

    /// Drop the conversation history. Also drops the applied-patch set
    /// (so a new conversation can re-apply textually identical
    /// proposals) and notifies the active provider — Claude CLI uses
    /// the hook to forget its `--resume` session id so the next message
    /// starts a fresh exchange on the backend.
    void clear() {
        m_history.clear();
        m_appliedPatches.clear();
        if (m_provider != nullptr) {
            m_provider->resetSession();
        }
    }

    /// The set of files/items attached to the conversation as context.
    [[nodiscard]] auto context() -> AiContext& { return m_context; }
    [[nodiscard]] auto context() const -> const AiContext& { return m_context; }

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

    /// Apply a SEARCH/REPLACE patch to the active document, wrapping
    /// the edit in a Scintilla undo action. Returns `true` on success,
    /// `false` when no document is active or the SEARCH text was not
    /// located.
    ///
    /// `recordAlways` selects how the attempt is folded into the
    /// applied-set:
    /// - `false` (manual button): record only on success, so the user
    ///   can fix the buffer and retry a failed apply.
    /// - `true` (live-edit): record every attempt so a failed apply
    ///   doesn't retry on every streamed chunk.
    auto applyPatch(const wxString& search, const wxString& replace, bool recordAlways = false) -> bool;

    /// True when `(search, replace)` has already been attempted this
    /// session. Drives the chat view's "applied" overlay.
    [[nodiscard]] auto isPatchApplied(const wxString& search, const wxString& replace) const -> bool;

private:
    /// `std::size_t` hash key for a `(search, replace)` pair. In-memory
    /// only, so `std::hash`'s run-to-run instability is harmless.
    /// Collisions degrade live-edit (one patch may be silently skipped
    /// because its hash matches an earlier one) without corrupting any
    /// data, so the trade-off vs. storing the full text is one-sided.
    /// Returns `size_t` rather than `uint64_t` so it stays the natural
    /// width on both 32- and 64-bit targets.
    [[nodiscard]] static auto patchKey(const wxString& search, const wxString& replace) -> std::size_t;

    Context& m_ctx;                                   ///< Application context.
    std::unique_ptr<AiProvider> m_provider;           ///< Active backend (null until configured).
    std::vector<AiMessage> m_history;                 ///< Conversation messages.
    AiContext m_context;                              ///< Files attached as context.
    wxString m_model;                                 ///< Model name sent with each request.
    wxString m_systemPrompt;                          ///< Configured system prompt (may be empty).
    std::unordered_set<std::size_t> m_appliedPatches; ///< Hashes of patches already attempted this session.

    // In-flight request state. Stored as members rather than captured by
    // the lambdas handed to the provider, so each lambda captures only
    // `this` (SBO-friendly) and doesn't heap-allocate the std::function.
    wxString m_pendingAccumulator;               ///< Streamed deltas so far for the in-flight request.
    AiProvider::ChunkHandler m_pendingOnChunk;   ///< Forwarded to the caller as deltas arrive.
    AiProvider::ResponseHandler m_pendingOnDone; ///< Forwarded to the caller on completion.

    bool m_agentMode = false; ///< Agent mode toggle state.
    bool m_liveEdit = false;  ///< Live-edit auto-apply toggle state.
};

} // namespace fbide::ai
