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

/// Strip an optional trailing slash from the endpoint URL.
auto trimTrailingSlash(wxString endpoint) -> wxString {
    if (endpoint.EndsWith("/")) {
        endpoint.RemoveLast();
    }
    return endpoint;
}

} // namespace

OllamaProvider::OllamaProvider(wxString endpoint)
: m_endpoint(trimTrailingSlash(std::move(endpoint))) {}

auto OllamaProvider::buildUrl(const AiRequest& /*request*/) const -> wxString {
    return m_endpoint + "/api/chat";
}

void OllamaProvider::applyHeaders(wxWebRequest& /*request*/) const {
    // Ollama needs no authentication header — the local server accepts
    // anything on the configured bind address.
}

auto OllamaProvider::buildBody(const AiRequest& request) const -> std::string {
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
            { "role", ollamaRoleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);
    return body.dump();
}

void OllamaProvider::parseLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) const {
    parseOllamaLine(line, onDelta, onError);
}

auto OllamaProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Ollama error (HTTP %d).", status);
}

auto OllamaProvider::requestFailedMessage(const wxString& detail) const -> wxString {
    // Most Ollama "request failed" outcomes are "no server listening" —
    // surface that hint so the user doesn't have to guess.
    return "Request failed: " + detail + " (is the Ollama server running?)";
}
