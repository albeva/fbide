//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <string>
#include <wx/webrequest.h>
#include "AiProvider.hpp"

namespace fbide::ai {

/**
 * AI provider backed by the Anthropic Messages API.
 *
 * Uses `wxWebRequest` for the HTTP round-trip with server-sent-events
 * streaming: the reply text is delivered incrementally as it arrives.
 * Handles one in-flight request at a time — overlapping `send` calls
 * fail fast.
 *
 * **Threading:** UI thread only.
 */
class AnthropicProvider final : public wxEvtHandler, public AiProvider {
public:
    NO_COPY_AND_MOVE(AnthropicProvider)

    /// Construct with the Anthropic API key (`x-api-key` header value).
    explicit AnthropicProvider(wxString apiKey);
    ~AnthropicProvider() override;

    /// Send `request` to the Messages API. See `AiProvider::send`.
    void send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) override;

private:
    /// `wxWebRequest` state transition — terminal states finish the request.
    void onRequestState(wxWebRequestEvent& event);
    /// Incoming response body chunk — buffered and parsed for SSE deltas.
    void onRequestData(wxWebRequestEvent& event);
    /// Parse whatever complete SSE lines are buffered, emitting text deltas.
    void consumeBuffer();
    /// Invoke the completion handler exactly once and reset request state.
    void finish(AiResponse response);

    wxString m_apiKey;            ///< Anthropic API key.
    wxWebRequest m_request;       ///< Current request — one at a time.
    ChunkHandler m_onChunk;       ///< Streaming delta callback.
    ResponseHandler m_onComplete; ///< Pending completion callback.
    std::string m_buffer;         ///< Unparsed SSE bytes.
    wxString m_streamError;       ///< Error reported via a stream `error` event.
    bool m_busy = false;          ///< True while a request is in flight.
};

} // namespace fbide::ai
