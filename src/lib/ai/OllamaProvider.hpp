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

namespace fbide {

/**
 * AI provider backed by a local Ollama server (`/api/chat`).
 *
 * Talks to Ollama over `wxWebRequest`, no API key. The reply is streamed:
 * Ollama returns newline-delimited JSON, one chunk per line, delivered
 * incrementally. Free and offline — the model runs on the user's machine.
 * Handles one in-flight request at a time.
 *
 * **Threading:** UI thread only.
 */
class OllamaProvider final : public wxEvtHandler, public AiProvider {
public:
    NO_COPY_AND_MOVE(OllamaProvider)

    /// Construct with the Ollama server base URL (e.g. `http://localhost:11434`).
    explicit OllamaProvider(wxString endpoint);
    ~OllamaProvider() override;

    /// Send `request` to the Ollama chat endpoint. See `AiProvider::send`.
    void send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) override;

private:
    /// `wxWebRequest` state transition — terminal states finish the request.
    void onRequestState(wxWebRequestEvent& event);
    /// Incoming response body chunk — buffered and parsed for NDJSON deltas.
    void onRequestData(wxWebRequestEvent& event);
    /// Parse whatever complete NDJSON lines are buffered, emitting deltas.
    void consumeBuffer();
    /// Invoke the completion handler exactly once and reset request state.
    void finish(AiResponse response);

    wxString m_endpoint;          ///< Ollama server base URL (no trailing slash).
    wxWebRequest m_request;       ///< Current request — one at a time.
    ChunkHandler m_onChunk;       ///< Streaming delta callback.
    ResponseHandler m_onComplete; ///< Pending completion callback.
    std::string m_buffer;         ///< Unparsed NDJSON bytes.
    wxString m_streamError;       ///< Error reported in a response chunk.
    bool m_busy = false;          ///< True while a request is in flight.
};

} // namespace fbide
