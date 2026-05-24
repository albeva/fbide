//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiProvider.hpp"

namespace fbide::ai {

/**
 * Offline AI provider for testing the chat UI.
 *
 * Ignores the request and streams back a fixed example reply (markdown
 * prose plus a FreeBASIC code sample) chunk by chunk on a timer — no
 * network, no API key, no cost. Exercises the streaming render path,
 * markdown rendering, and code-block highlighting / reformatting.
 *
 * **Threading:** UI thread only.
 */
class MockProvider final : public wxEvtHandler, public AiProvider {
public:
    NO_COPY_AND_MOVE(MockProvider)

    /// One picked canned reply — the text and whether to skip chunking.
    /// Returned by `pickReply` and consumed by `send`. Exposed so tests
    /// can verify the dispatch table without running the timer.
    struct PickedReply {
        wxString text;     ///< Full canned reply body.
        bool fast = false; ///< True for `allf` / `all fast` — emit instantly.
    };

    MockProvider();

    /// Stream the canned example reply. See `AiProvider::send`.
    void send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) override;

    /// Map `request`'s last message onto the matching canned reply.
    /// Anything that doesn't match a known command falls through to the
    /// default mixed reply.
    [[nodiscard]] static auto pickReply(const AiRequest& request) -> PickedReply;

private:
    /// Timer tick — emit the next chunk, or finish the request.
    void onTick(wxTimerEvent& event);

    wxTimer m_timer;                ///< Drives chunked emission.
    std::vector<wxString> m_chunks; ///< Canned reply, pre-sliced into chunks.
    std::size_t m_index = 0;        ///< Index of the next chunk to emit.
    ChunkHandler m_onChunk;         ///< Streaming delta callback.
    ResponseHandler m_onComplete;   ///< Pending completion callback.
    bool m_busy = false;            ///< True while emitting.
};

} // namespace fbide::ai
