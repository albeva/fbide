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

/// Borrow the string at `key` from `node` as a view over nlohmann's
/// internal storage. Returns an empty view when the key is absent or
/// the value isn't a string. Used on the per-token hot path so each
/// streamed delta allocates only the wxString.
auto borrowString(const json& node, const char* key) -> std::string_view {
    const auto it = node.find(key);
    if (it == node.end()) {
        return {};
    }
    const auto* str = it->get_ptr<const json::string_t*>();
    if (str == nullptr) {
        return {};
    }
    return *str;
}

} // namespace

OllamaProvider::OllamaProvider(wxString endpoint)
: m_endpoint(trimTrailingSlash(std::move(endpoint))) {}

auto OllamaProvider::roleToString(const AiRole role) -> const char* {
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

void OllamaProvider::parseStreamLine(const std::string_view line, StreamLineConsumer& sink) {
    const auto chunk = json::parse(line, nullptr, false);
    if (chunk.is_discarded()) {
        return;
    }
    if (chunk.contains("error")) {
        const auto message = borrowString(chunk, "error");
        sink.onError(message.empty()
                         ? wxString { "Unknown Ollama error." }
                         : wxString::FromUTF8(message.data(), message.size()));
        return;
    }
    const auto messageIt = chunk.find("message");
    if (messageIt != chunk.end() && messageIt->is_object()) {
        const auto content = borrowString(*messageIt, "content");
        sink.onDelta(wxString::FromUTF8(content.data(), content.size()));
    }
}

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
            { "role", roleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);
    return body.dump();
}

void OllamaProvider::parseLine(const std::string_view line, StreamLineConsumer& sink) const {
    parseStreamLine(line, sink);
}

auto OllamaProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Ollama error (HTTP %d).", status);
}

auto OllamaProvider::unauthorizedMessage() const -> wxString {
    // Ollama doesn't authenticate by default — a 401 means something is
    // sitting in front of the server (reverse proxy, ingress, etc).
    return "Ollama returned Unauthorized — is a proxy in front of the server requiring credentials?";
}

auto OllamaProvider::requestFailedMessage(const wxString& detail) const -> wxString {
    // Most Ollama "request failed" outcomes are "no server listening" —
    // surface that hint so the user doesn't have to guess.
    return "Request failed: " + detail + " (is the Ollama server running?)";
}
