//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "GeminiProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {
constexpr int kHttpErrorStatus = 400;

/// Map a role onto the Gemini `contents[].role` value. Gemini uses
/// `model` (not `assistant`) and carries the system prompt separately,
/// so a `System` role never appears here — treat it as `user`.
auto roleToString(AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "model";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}

/// Build the streaming endpoint URL for `model`.
auto streamUrl(const wxString& model) -> wxString {
    return wxString::Format(
        "https://generativelanguage.googleapis.com/v1beta/models/%s:streamGenerateContent?alt=sse",
        model
    );
}
} // namespace

GeminiProvider::GeminiProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {
    Bind(wxEVT_WEBREQUEST_STATE, &GeminiProvider::onRequestState, this);
    Bind(wxEVT_WEBREQUEST_DATA, &GeminiProvider::onRequestData, this);
}

GeminiProvider::~GeminiProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void GeminiProvider::send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    json body;
    auto contents = json::array();
    for (const auto& msg : request.messages) {
        contents.push_back({
            { "role", roleToString(msg.role) },
            { "parts", json::array({ json { { "text", msg.content.utf8_string() } } }) },
        });
    }
    body["contents"] = std::move(contents);
    if (!request.system.empty()) {
        body["systemInstruction"] = {
            { "parts", json::array({ json { { "text", request.system.utf8_string() } } }) },
        };
    }

    m_request = wxWebSession::GetDefault().CreateRequest(this, streamUrl(request.model));
    if (!m_request.IsOk()) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetMethod("POST");
    m_request.SetHeader("x-goog-api-key", m_apiKey);
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

void GeminiProvider::onRequestData(wxWebRequestEvent& event) {
    m_buffer.append(static_cast<const char*>(event.GetDataBuffer()), event.GetDataSize());
    consumeBuffer();
}

void GeminiProvider::consumeBuffer() {
    // SSE: each payload arrives on a `data:` line, one JSON object each.
    for (auto pos = m_buffer.find('\n'); pos != std::string::npos; pos = m_buffer.find('\n')) {
        std::string line = m_buffer.substr(0, pos);
        m_buffer.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.starts_with("data:")) {
            continue;
        }

        const auto payload = json::parse(line.substr(std::string_view("data:").size()), nullptr, false);
        if (payload.is_discarded()) {
            continue;
        }
        if (payload.contains("error")) {
            const auto& error = payload["error"];
            m_streamError = wxString::FromUTF8(error.is_object() ? error.value("message", "Unknown API error.") : "Unknown API error.");
            continue;
        }
        // candidates[0].content.parts[*].text
        if (!payload.contains("candidates") || !payload["candidates"].is_array() || payload["candidates"].empty()) {
            continue;
        }
        const auto& content = payload["candidates"][0]["content"];
        if (!content.is_object() || !content["parts"].is_array()) {
            continue;
        }
        for (const auto& part : content["parts"]) {
            if (part.contains("text")) {
                m_onChunk(wxString::FromUTF8(part.value("text", "")));
            }
        }
    }
}

void GeminiProvider::onRequestState(wxWebRequestEvent& event) {
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        consumeBuffer();
        AiResponse response;
        if (const int status = event.GetResponse().GetStatus(); status >= kHttpErrorStatus) {
            response.error = wxString::Format("Gemini API error (HTTP %d).", status);
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
        finish(AiResponse { .ok = false, .text = {}, .error = "Unauthorized - check the Gemini API key." });
        break;
    case wxWebRequest::State_Cancelled:
        finish(AiResponse { .ok = false, .text = {}, .error = "Request cancelled." });
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        break; // Not a terminal state — nothing to report yet.
    }
}

void GeminiProvider::finish(AiResponse response) {
    m_busy = false;
    m_buffer.clear();
    m_onChunk = nullptr;
    if (auto handler = std::exchange(m_onComplete, nullptr)) {
        handler(std::move(response));
    }
}
