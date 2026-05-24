//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AnthropicProvider.hpp"
#include <nlohmann/json.hpp>
#include "StreamParsers.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {
constexpr auto kEndpoint = "https://api.anthropic.com/v1/messages";
constexpr auto kAnthropicVersion = "2023-06-01";
constexpr int kHttpErrorStatus = 400;
} // namespace

AnthropicProvider::AnthropicProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {
    Bind(wxEVT_WEBREQUEST_STATE, &AnthropicProvider::onRequestState, this);
    Bind(wxEVT_WEBREQUEST_DATA, &AnthropicProvider::onRequestData, this);
}

AnthropicProvider::~AnthropicProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void AnthropicProvider::send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    json body;
    body["model"] = request.model.utf8_string();
    body["max_tokens"] = request.maxTokens;
    body["stream"] = true;
    if (!request.system.empty()) {
        body["system"] = request.system.utf8_string();
    }
    auto messages = json::array();
    for (const auto& msg : request.messages) {
        messages.push_back({
            { "role", anthropicRoleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);

    m_request = wxWebSession::GetDefault().CreateRequest(this, kEndpoint);
    if (!m_request.IsOk()) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetMethod("POST");
    m_request.SetHeader("x-api-key", m_apiKey);
    m_request.SetHeader("anthropic-version", kAnthropicVersion);
    m_request.SetData(wxString::FromUTF8(body.dump()), "application/json");
    // Stream the body to us through wxEVT_WEBREQUEST_DATA events.
    m_request.SetStorage(wxWebRequest::Storage_None);

    m_onChunk = std::move(onChunk);
    m_onComplete = std::move(onComplete);
    m_buffer.clear();
    m_streamError.clear();
    m_busy = true;
    m_request.Start();
}

void AnthropicProvider::onRequestData(wxWebRequestEvent& event) {
    m_buffer.append(static_cast<const char*>(event.GetDataBuffer()), event.GetDataSize());
    consumeBuffer();
}

void AnthropicProvider::consumeBuffer() {
    // SSE: every payload arrives on a `data:` line; `event:` lines and
    // blank separators are ignored. Anthropic puts one JSON object per
    // `data:` line. Per-line parsing lives in `parseAnthropicLine`; the
    // loop here just splits on `\n` and strips the optional `\r`.
    const auto onError = [this](const wxString& message) { m_streamError = message; };
    for (auto pos = m_buffer.find('\n'); pos != std::string::npos; pos = m_buffer.find('\n')) {
        std::string line = m_buffer.substr(0, pos);
        m_buffer.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        parseAnthropicLine(line, m_onChunk, onError);
    }
}

void AnthropicProvider::onRequestState(wxWebRequestEvent& event) {
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        consumeBuffer();
        AiResponse response;
        if (const int status = event.GetResponse().GetStatus(); status >= kHttpErrorStatus) {
            response.error = wxString::Format("Anthropic API error (HTTP %d).", status);
        } else if (!m_streamError.empty()) {
            response.error = m_streamError;
        } else {
            response.ok = true;
        }
        finish(std::move(response));
        break;
    }
    case wxWebRequest::State_Failed:
        finish(AiResponse { .ok = false, .text = {}, .error = "Request failed: " + event.GetErrorDescription() });
        break;
    case wxWebRequest::State_Unauthorized:
        finish(AiResponse { .ok = false, .text = {}, .error = "Unauthorized - check the Anthropic API key." });
        break;
    case wxWebRequest::State_Cancelled:
        finish(AiResponse { .ok = false, .text = {}, .error = "Request cancelled." });
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        break; // Not a terminal state — nothing to report yet.
    }
}

void AnthropicProvider::finish(AiResponse response) {
    m_busy = false;
    m_buffer.clear();
    m_onChunk = nullptr;
    if (auto handler = std::exchange(m_onComplete, nullptr)) {
        handler(std::move(response));
    }
}
