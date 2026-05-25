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

constexpr std::string_view kSsePrefix = "data:";

/// Read `error.message` from a payload that carries either an object
/// or a bare value. Falls back to a generic string.
auto extractErrorMessage(const json& error) -> wxString {
    if (error.is_object()) {
        return wxString::FromUTF8(error.value("message", "Unknown API error."));
    }
    return "Unknown API error.";
}

} // namespace

GeminiProvider::GeminiProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {}

auto GeminiProvider::roleToString(const AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "model";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}

void GeminiProvider::parseStreamLine(const std::string_view line, StreamLineConsumer& sink) {
    if (!line.starts_with(kSsePrefix)) {
        return;
    }
    const auto payload = json::parse(line.substr(kSsePrefix.size()), nullptr, false);
    if (payload.is_discarded()) {
        return;
    }
    if (payload.contains("error")) {
        sink.onError(extractErrorMessage(payload["error"]));
        return;
    }
    if (!payload.contains("candidates") || !payload["candidates"].is_array() || payload["candidates"].empty()) {
        return;
    }
    const auto& content = payload["candidates"][0]["content"];
    if (!content.is_object() || !content["parts"].is_array()) {
        return;
    }
    for (const auto& part : content["parts"]) {
        if (part.contains("text")) {
            sink.onDelta(wxString::FromUTF8(part.value("text", "")));
        }
    }
}

auto GeminiProvider::buildUrl(const AiRequest& request) const -> wxString {
    return wxString::Format(
        "https://generativelanguage.googleapis.com/v1beta/models/%s:streamGenerateContent?alt=sse",
        request.model
    );
}

void GeminiProvider::applyHeaders(wxWebRequest& request) const {
    request.SetHeader("x-goog-api-key", m_apiKey);
}

auto GeminiProvider::buildBody(const AiRequest& request) const -> std::string {
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
    return body.dump();
}

void GeminiProvider::parseLine(const std::string_view line, StreamLineConsumer& sink) const {
    parseStreamLine(line, sink);
}

auto GeminiProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Gemini API error (HTTP %d).", status);
}

auto GeminiProvider::unauthorizedMessage() const -> wxString {
    return "Unauthorized - check the Gemini API key.";
}

auto GeminiProvider::requestFailedMessage(const wxString& detail) const -> wxString {
    return "Gemini request failed: " + detail;
}
