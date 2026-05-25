//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "WebStreamProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {
constexpr int kHttpErrorStatus = 400;

/// Compact when the consumed cursor crosses both an absolute byte
/// threshold and a fraction of the buffer — both checks together keep
/// the small-stream case from compacting on every line while still
/// bounding total memory for long streams.
constexpr std::size_t kCompactBytes = 4096;
constexpr std::size_t kCompactDivisor = 2;
} // namespace

WebStreamProvider::WebStreamProvider() {
    Bind(wxEVT_WEBREQUEST_STATE, &WebStreamProvider::onRequestState, this);
    Bind(wxEVT_WEBREQUEST_DATA, &WebStreamProvider::onRequestData, this);
}

WebStreamProvider::~WebStreamProvider() {
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void WebStreamProvider::send(const AiRequest& request, ChunkHandler onChunk, ToolCallHandler onToolCall, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    const auto body = buildBody(request);

    m_request = wxWebSession::GetDefault().CreateRequest(this, buildUrl(request));
    if (!m_request.IsOk()) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "Failed to create the web request." });
        return;
    }
    m_request.SetMethod("POST");
    applyHeaders(m_request);
    m_request.SetData(wxString::FromUTF8(body), "application/json");
    // Stream the body to us through wxEVT_WEBREQUEST_DATA events.
    m_request.SetStorage(wxWebRequest::Storage_None);

    m_onChunk = std::move(onChunk);
    m_onToolCall = std::move(onToolCall);
    m_onComplete = std::move(onComplete);
    m_buffer.clear();
    m_consumed = 0;
    m_streamError.clear();
    m_busy = true;
    m_request.Start();
}

void WebStreamProvider::onRequestData(wxWebRequestEvent& event) {
    m_buffer.append(static_cast<const char*>(event.GetDataBuffer()), event.GetDataSize());
    consumeBuffer();
}

void WebStreamProvider::consumeBuffer() {
    while (true) {
        const auto pos = m_buffer.find('\n', m_consumed);
        if (pos == std::string::npos) {
            break;
        }
        auto line = std::string_view(m_buffer).substr(m_consumed, pos - m_consumed);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        parseLine(line, m_sink);
        m_consumed = pos + 1;
    }
    // Compact the prefix once we've consumed enough — keeps total memory
    // bounded for long replies without paying an `erase` per line.
    if (m_consumed >= kCompactBytes && m_consumed >= m_buffer.size() / kCompactDivisor) {
        m_buffer.erase(0, m_consumed);
        m_consumed = 0;
    }
}

void WebStreamProvider::Sink::onDelta(const wxString& delta) {
    if (m_owner.m_onChunk) {
        m_owner.m_onChunk(delta);
    }
}

void WebStreamProvider::Sink::onError(const wxString& message) {
    m_owner.m_streamError = message;
}

void WebStreamProvider::Sink::onToolCall(AiToolCall call) {
    if (m_owner.m_onToolCall) {
        m_owner.m_onToolCall(std::move(call));
    }
}

void WebStreamProvider::onRequestState(wxWebRequestEvent& event) {
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        consumeBuffer();
        AiResponse response;
        if (const int status = event.GetResponse().GetStatus(); status >= kHttpErrorStatus) {
            response.error = httpErrorMessage(status);
        } else if (!m_streamError.empty()) {
            response.error = m_streamError;
        } else {
            response.ok = true;
        }
        finish(std::move(response));
        break;
    }
    case wxWebRequest::State_Failed:
        finish(AiResponse { .ok = false, .text = {}, .error = requestFailedMessage(event.GetErrorDescription()) });
        break;
    case wxWebRequest::State_Unauthorized:
        finish(AiResponse { .ok = false, .text = {}, .error = unauthorizedMessage() });
        break;
    case wxWebRequest::State_Cancelled:
        finish(AiResponse { .ok = false, .text = {}, .error = "Request cancelled." });
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        break; // Not a terminal state — nothing to report yet.
    }
}

void WebStreamProvider::cancel() {
    if (!m_busy) {
        return;
    }
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        m_request.Cancel();
    }
}

void WebStreamProvider::finish(AiResponse response) {
    m_busy = false;
    m_buffer.clear();
    m_consumed = 0;
    m_onChunk = nullptr;
    m_onToolCall = nullptr;
    if (auto handler = std::exchange(m_onComplete, nullptr)) {
        handler(std::move(response));
    }
}
