//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AnthropicProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using json = nlohmann::json;

namespace {
constexpr auto kEndpoint = "https://api.anthropic.com/v1/messages";
constexpr auto kAnthropicVersion = "2023-06-01";

/// Map a role onto the Anthropic `messages[].role` value. Anthropic carries
/// the system prompt as a top-level field, so a `System` role never appears
/// in the messages array — treat it as `user` defensively.
auto roleToString(AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "assistant";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}
} // namespace

AnthropicProvider::AnthropicProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {
    Bind(wxEVT_WEBREQUEST_STATE, &AnthropicProvider::onRequestState, this);
}

AnthropicProvider::~AnthropicProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void AnthropicProvider::send(const AiRequest& request, ResponseHandler handler) {
    if (m_busy) {
        handler(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    json body;
    body["model"] = request.model.utf8_string();
    body["max_tokens"] = request.maxTokens;
    if (!request.system.empty()) {
        body["system"] = request.system.utf8_string();
    }
    auto messages = json::array();
    for (const auto& msg : request.messages) {
        messages.push_back({
            { "role", roleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);

    m_request = wxWebSession::GetDefault().CreateRequest(this, kEndpoint);
    if (!m_request.IsOk()) {
        handler(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetMethod("POST");
    m_request.SetHeader("x-api-key", m_apiKey);
    m_request.SetHeader("anthropic-version", kAnthropicVersion);
    m_request.SetData(wxString::FromUTF8(body.dump()), "application/json");

    m_handler = std::move(handler);
    m_busy = true;
    m_request.Start();
}

void AnthropicProvider::onRequestState(wxWebRequestEvent& event) {
    AiResponse response;
    switch (event.GetState()) {
    case wxWebRequest::State_Completed:
        response = parseResponse(event.GetResponse().AsString());
        break;
    case wxWebRequest::State_Failed:
        response.error = "Request failed: " + event.GetErrorDescription();
        break;
    case wxWebRequest::State_Unauthorized:
        response.error = "Unauthorized — check the Anthropic API key.";
        break;
    case wxWebRequest::State_Cancelled:
        response.error = "Request cancelled.";
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        return; // Not a terminal state — nothing to report yet.
    }

    m_busy = false;
    if (auto handler = std::exchange(m_handler, nullptr)) {
        handler(std::move(response));
    }
}

auto AnthropicProvider::parseResponse(const wxString& raw) -> AiResponse {
    AiResponse response;

    const auto parsed = json::parse(raw.utf8_string(), nullptr, false);
    if (parsed.is_discarded()) {
        response.error = "Malformed JSON in the API response.";
        return response;
    }

    // Anthropic reports API-level failures as `{ "error": { "message": ... } }`.
    if (parsed.contains("error")) {
        const auto& error = parsed["error"];
        response.error = wxString::FromUTF8(error.value("message", "Unknown API error."));
        return response;
    }

    // Success: `content` is an array of blocks; collect the text ones.
    if (parsed.contains("content") && parsed["content"].is_array()) {
        for (const auto& block : parsed["content"]) {
            if (block.value("type", "") == "text") {
                response.text += wxString::FromUTF8(block.value("text", ""));
            }
        }
        response.ok = true;
        return response;
    }

    response.error = "Unexpected API response shape.";
    return response;
}
