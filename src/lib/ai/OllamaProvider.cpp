//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "OllamaProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using json = nlohmann::json;

namespace {
/// Map a role onto the Ollama `messages[].role` value. Ollama carries the
/// system prompt as a `system`-role message in the array.
auto roleToString(const AiRole role) -> const char* {
    switch (role) {
    case AiRole::System:
        return "system";
    case AiRole::Assistant:
        return "assistant";
    case AiRole::User:
        break;
    }
    return "user";
}

/// Join the server base URL with the chat endpoint path, tolerating a
/// trailing slash on the configured endpoint.
auto chatUrl(wxString endpoint) -> wxString {
    if (endpoint.EndsWith("/")) {
        endpoint.RemoveLast();
    }
    return endpoint + "/api/chat";
}
} // namespace

OllamaProvider::OllamaProvider(wxString endpoint)
: m_endpoint(std::move(endpoint)) {
    Bind(wxEVT_WEBREQUEST_STATE, &OllamaProvider::onRequestState, this);
}

OllamaProvider::~OllamaProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void OllamaProvider::send(const AiRequest& request, ResponseHandler handler) {
    if (m_busy) {
        handler(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    json body;
    body["model"] = request.model.utf8_string();
    body["stream"] = false;
    auto messages = json::array();
    if (!request.system.empty()) {
        messages.push_back({
            { "role", "system" },
            { "content", request.system.utf8_string() },
        });
    }
    for (const auto& msg : request.messages) {
        messages.push_back({
            { "role", roleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);

    m_request = wxWebSession::GetDefault().CreateRequest(this, chatUrl(m_endpoint));
    if (!m_request.IsOk()) {
        handler(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetTimeouts(
        300000, // 5 min connect timeout
        300000  // 5 min transfer timeout
    );
    m_request.SetMethod("POST");
    m_request.SetData(wxString::FromUTF8(body.dump()), "application/json");

    m_handler = std::move(handler);
    m_busy = true;
    m_request.Start();
}

void OllamaProvider::onRequestState(wxWebRequestEvent& event) {
    AiResponse response;
    switch (event.GetState()) {
    case wxWebRequest::State_Completed:
        response = parseResponse(event.GetResponse().AsString());
        break;
    case wxWebRequest::State_Failed:
        response.error = "Request failed: " + event.GetErrorDescription()
                       + " (is the Ollama server running?)";
        break;
    case wxWebRequest::State_Unauthorized:
        response.error = "Unauthorized.";
        break;
    case wxWebRequest::State_Cancelled:
        response.error = "Request cancelled.";
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        return; // Not a terminal state — nothing to report yet.
    }

    m_busy = false;
    if (const auto handler = std::exchange(m_handler, nullptr)) {
        handler(std::move(response));
    }
}

auto OllamaProvider::parseResponse(const wxString& raw) -> AiResponse {
    AiResponse response;

    const auto parsed = json::parse(raw.utf8_string(), nullptr, false);
    if (parsed.is_discarded()) {
        response.error = "Malformed JSON in the Ollama response.";
        return response;
    }

    // Ollama reports failures as `{ "error": "<message>" }`.
    if (parsed.contains("error")) {
        response.error = wxString::FromUTF8(parsed.value("error", "Unknown Ollama error."));
        return response;
    }

    // Success: `{ "message": { "role": ..., "content": ... } }`.
    if (parsed.contains("message") && parsed["message"].is_object()) {
        response.text = wxString::FromUTF8(parsed["message"].value("content", ""));
        response.ok = true;
        return response;
    }

    response.error = "Unexpected Ollama response shape.";
    return response;
}
