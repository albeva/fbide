//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "OllamaProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {
constexpr int kHttpErrorStatus = 400;

/// Map a role onto the Ollama `messages[].role` value. Ollama carries the
/// system prompt as a `system`-role message in the array.
auto roleToString(AiRole role) -> const char* {
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
    Bind(wxEVT_WEBREQUEST_DATA, &OllamaProvider::onRequestData, this);
}

OllamaProvider::~OllamaProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void OllamaProvider::send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    json body;
    body["model"] = request.model.utf8_string();
    body["stream"] = true;
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
        onComplete(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetMethod("POST");
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

void OllamaProvider::onRequestData(wxWebRequestEvent& event) {
    m_buffer.append(static_cast<const char*>(event.GetDataBuffer()), event.GetDataSize());
    consumeBuffer();
}

void OllamaProvider::consumeBuffer() {
    // Ollama streams newline-delimited JSON — one object per line, e.g.
    // {"message":{"content":"<delta>"},"done":false}.
    for (auto pos = m_buffer.find('\n'); pos != std::string::npos; pos = m_buffer.find('\n')) {
        std::string line = m_buffer.substr(0, pos);
        m_buffer.erase(0, pos + 1);

        const auto chunk = json::parse(line, nullptr, false);
        if (chunk.is_discarded()) {
            continue;
        }
        if (chunk.contains("error")) {
            m_streamError = wxString::FromUTF8(chunk.value("error", "Unknown Ollama error."));
            continue;
        }
        if (chunk.contains("message") && chunk["message"].is_object()) {
            m_onChunk(wxString::FromUTF8(chunk["message"].value("content", "")));
        }
    }
}

void OllamaProvider::onRequestState(wxWebRequestEvent& event) {
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        consumeBuffer();
        AiResponse response;
        if (const int status = event.GetResponse().GetStatus(); status >= kHttpErrorStatus) {
            response.error = wxString::Format("Ollama error (HTTP %d).", status);
        } else if (!m_streamError.empty()) {
            response.error = m_streamError;
        } else {
            response.ok = true;
        }
        finish(std::move(response));
        break;
    }
    case wxWebRequest::State_Failed:
        finish(AiResponse {
            .ok = false,
            .text = {},
            .error = "Request failed: " + event.GetErrorDescription() + " (is the Ollama server running?)",
        });
        break;
    case wxWebRequest::State_Unauthorized:
        finish(AiResponse { .ok = false, .text = {}, .error = "Unauthorized." });
        break;
    case wxWebRequest::State_Cancelled:
        finish(AiResponse { .ok = false, .text = {}, .error = "Request cancelled." });
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        break; // Not a terminal state — nothing to report yet.
    }
}

void OllamaProvider::finish(AiResponse response) {
    m_busy = false;
    m_buffer.clear();
    m_onChunk = nullptr;
    if (auto handler = std::exchange(m_onComplete, nullptr)) {
        handler(std::move(response));
    }
}
