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

/// Borrow the string at `key` from `node` as a view over nlohmann's
/// internal storage. Returns an empty view when the key is absent or
/// the value isn't a string — matches the semantics of
/// `node.value(key, "")` but skips the std::string copy that the
/// by-value form makes. Used on the per-token hot path so each
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

/// Read `error.message` from a payload that carries either an object
/// (`{"message":"...", ...}`) or a bare value. Falls back to a generic
/// string so the user always gets something readable.
auto extractErrorMessage(const json& error) -> wxString {
    if (error.is_object()) {
        const auto message = borrowString(error, "message");
        if (!message.empty()) {
            return wxString::FromUTF8(message.data(), message.size());
        }
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
    const auto type = borrowString(payload, "type");
    if (type == "content_block_delta") {
        const auto& delta = payload["delta"];
        if (delta.is_object() && borrowString(delta, "type") == "text_delta") {
            const auto text = borrowString(delta, "text");
            sink.onDelta(wxString::FromUTF8(text.data(), text.size()));
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

auto AnthropicProvider::serializeBody(const AiRequest& request) -> std::string {
    json body;
    body["model"] = request.model.utf8_string();
    body["max_tokens"] = request.maxTokens;
    body["stream"] = true;
    // If any block is cacheable, emit the array form with cache_control
    // breakpoints (up to `kMaxCacheBreakpoints`) on the first cacheable
    // blocks. Otherwise collapse to the simple string form — keeps the
    // wire identical to the pre-caching path for short prompts.
    auto hasCacheable = std::ranges::any_of(request.system, [](const AiContent& block) {
        return block.cacheable && !block.text.empty();
    });
    if (hasCacheable) {
        auto systemArray = json::array();
        int budget = kMaxCacheBreakpoints;
        for (const auto& block : request.system) {
            if (block.text.empty()) {
                continue;
            }
            json entry {
                { "type", "text" },
                { "text", block.text.utf8_string() },
            };
            if (block.cacheable && budget > 0) {
                entry["cache_control"] = { { "type", "ephemeral" } };
                budget--;
            }
            systemArray.push_back(std::move(entry));
        }
        if (!systemArray.empty()) {
            body["system"] = std::move(systemArray);
        }
    } else {
        const auto joined = joinSystem(request.system);
        if (!joined.empty()) {
            body["system"] = joined.utf8_string();
        }
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

auto AnthropicProvider::buildBody(const AiRequest& request) const -> std::string {
    return serializeBody(request);
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
