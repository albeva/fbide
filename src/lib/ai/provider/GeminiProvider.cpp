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

GeminiProvider::GeminiProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {}

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
            { "role", geminiRoleToString(msg.role) },
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

void GeminiProvider::parseLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) const {
    parseGeminiLine(line, onDelta, onError);
}

auto GeminiProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Gemini API error (HTTP %d).", status);
}

auto GeminiProvider::unauthorizedMessage() const -> wxString {
    return "Unauthorized - check the Gemini API key.";
}
