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
 * AI provider backed by the Anthropic Messages API.
 *
 * Uses `wxWebRequest` for the HTTP round-trip. Handles one in-flight
 * request at a time — overlapping `send` calls fail fast.
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
    void send(const AiRequest& request, ResponseHandler handler) override;

private:
    /// `wxWebRequest` state transition — completes the pending handler
    /// once the request reaches a terminal state.
    void onRequestState(wxWebRequestEvent& event);

    /// Map a raw JSON response body onto an `AiResponse`.
    static auto parseResponse(const wxString& raw) -> AiResponse;

    wxString m_apiKey;         ///< Anthropic API key.
    wxWebRequest m_request;    ///< Current request — one at a time.
    ResponseHandler m_handler; ///< Pending completion callback.
    bool m_busy = false;       ///< True while a request is in flight.
};

} // namespace fbide
