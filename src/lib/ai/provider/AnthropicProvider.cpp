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
} // namespace

AnthropicProvider::AnthropicProvider(wxString apiKey)
: m_apiKey(std::move(apiKey)) {}

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
            { "role", anthropicRoleToString(msg.role) },
            { "content", msg.content.utf8_string() },
        });
    }
    body["messages"] = std::move(messages);
    return body.dump();
}

void AnthropicProvider::parseLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) const {
    parseAnthropicLine(line, onDelta, onError);
}

auto AnthropicProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("Anthropic API error (HTTP %d).", status);
}

auto AnthropicProvider::unauthorizedMessage() const -> wxString {
    return "Unauthorized - check the Anthropic API key.";
}
