//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/webrequest.h>
#include "AiProvider.hpp"

namespace fbide {

/**
 * AI provider backed by a local Ollama server (`/api/chat`).
 *
 * Talks to Ollama over `wxWebRequest`, no API key. Free and offline —
 * the model runs on the user's machine. Handles one in-flight request
 * at a time.
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
    void send(const AiRequest& request, ResponseHandler handler) override;

private:
    /// `wxWebRequest` state transition — completes the pending handler
    /// once the request reaches a terminal state.
    void onRequestState(wxWebRequestEvent& event);

    /// Map a raw JSON response body onto an `AiResponse`.
    static auto parseResponse(const wxString& raw) -> AiResponse;

    wxString m_endpoint;       ///< Ollama server base URL (no trailing slash).
    wxWebRequest m_request;    ///< Current request — one at a time.
    ResponseHandler m_handler; ///< Pending completion callback.
    bool m_busy = false;       ///< True while a request is in flight.
};

} // namespace fbide
