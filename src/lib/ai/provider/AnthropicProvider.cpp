//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AnthropicProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr auto kEndpoint = "https://api.anthropic.com/v1/messages";
constexpr auto kAnthropicVersion = "2023-06-01";
constexpr std::string_view kSsePrefix = "data:";

/// Read `error.message` from a payload that carries either an object
/// (`{"message":"...", ...}`) or a bare value. Falls back to a generic
/// string so the user always gets something readable.
auto extractErrorMessage(const json& error) -> wxString {
    if (error.is_object()) {
        return wxString::FromUTF8(error.value("message", "Unknown API error."));
    }
    return "Unknown API error.";
}

} // namespace

AnthropicProvider::AnthropicProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {}

auto AnthropicProvider::roleToString(const AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "assistant";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}

void AnthropicProvider::parseStreamLine(const std::string_view line, StreamLineConsumer& sink) {
    if (!line.starts_with(kSsePrefix)) {
        return;
    }
    const auto payload = json::parse(line.substr(kSsePrefix.size()), nullptr, false);
    if (payload.is_discarded()) {
        return;
    }
    const auto type = payload.value("type", "");
    if (type == "content_block_delta") {
        const auto& delta = payload["delta"];
        if (delta.is_object() && delta.value("type", "") == "text_delta") {
            sink.onDelta(wxString::FromUTF8(delta.value("text", "")));
        }
    } else if (type == "error") {
        sink.onError(extractErrorMessage(payload["error"]));
    }
}

auto AnthropicProvider::buildUrl(const AiRequest& /*request*/) const -> wxString {
    return kEndpoint;
}

void AnthropicProvider::applyHeaders(wxWebRequest& request) const {
    request.SetHeader("x-api-key", m_apiKey);
    request.SetHeader("anthropic-version", kAnthropicVersion);
}

auto AnthropicProvider::buildBody(const AiRequest& request) const -> std::string {
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
            { "role", roleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);
    return body.dump();
}

void AnthropicProvider::parseLine(const std::string_view line, StreamLineConsumer& sink) const {
    parseStreamLine(line, sink);
}

auto AnthropicProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Anthropic API error (HTTP %d).", status);
}

auto AnthropicProvider::unauthorizedMessage() const -> wxString {
    return "Unauthorized - check the Anthropic API key.";
}

auto AnthropicProvider::requestFailedMessage(const wxString& detail) const -> wxString {
    return "Anthropic request failed: " + detail;
}
