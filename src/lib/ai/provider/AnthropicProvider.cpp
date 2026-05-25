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

void AnthropicProvider::parseStreamLine(const std::string_view line, ToolUseStates& states, StreamLineConsumer& sink) {
    if (!line.starts_with(kSsePrefix)) {
        return;
    }
    const auto payload = json::parse(line.substr(kSsePrefix.size()), nullptr, false);
    if (payload.is_discarded()) {
        return;
    }
    const auto type = borrowString(payload, "type");
    if (type == "message_start") {
        // Clear any per-block tool_use state from the previous message
        // so concurrent requests on the same provider (one at a time,
        // but reused) start fresh.
        states.clear();
        return;
    }
    if (type == "content_block_start") {
        const auto& block = payload["content_block"];
        if (block.is_object() && borrowString(block, "type") == "tool_use") {
            const auto blockIndex = payload.value("index", -1);
            if (blockIndex >= 0) {
                ToolUseState state;
                const auto id = borrowString(block, "id");
                const auto name = borrowString(block, "name");
                state.id = wxString::FromUTF8(id.data(), id.size());
                state.name = wxString::FromUTF8(name.data(), name.size());
                states[blockIndex] = std::move(state);
            }
        }
        return;
    }
    if (type == "content_block_delta") {
        const auto& delta = payload["delta"];
        if (!delta.is_object()) {
            return;
        }
        const auto deltaType = borrowString(delta, "type");
        if (deltaType == "text_delta") {
            const auto text = borrowString(delta, "text");
            sink.onDelta(wxString::FromUTF8(text.data(), text.size()));
            return;
        }
        if (deltaType == "input_json_delta") {
            const auto blockIndex = payload.value("index", -1);
            const auto it = states.find(blockIndex);
            if (it != states.end()) {
                const auto partial = borrowString(delta, "partial_json");
                it->second.json.append(partial);
            }
        }
        return;
    }
    if (type == "content_block_stop") {
        const auto blockIndex = payload.value("index", -1);
        const auto it = states.find(blockIndex);
        if (it != states.end()) {
            // Empty input is valid (tool takes no args); pass through.
            auto& state = it->second;
            sink.onToolCall(AiToolCall {
                .id = std::move(state.id),
                .name = std::move(state.name),
                .argumentsJson = wxString::FromUTF8(state.json),
            });
            states.erase(it);
        }
        return;
    }
    if (type == "error") {
        sink.onError(extractErrorMessage(payload["error"]));
    }
}

void AnthropicProvider::parseStreamLine(const std::string_view line, StreamLineConsumer& sink) {
    ToolUseStates discardedStates;
    parseStreamLine(line, discardedStates, sink);
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
        // Anthropic accepts message.content as either a string (text only)
        // or an array of content blocks (text + tool_use / tool_result).
        // Use the array form when the message carries any tool-related
        // payload; fall back to the cheaper string form otherwise so
        // historical text-only turns serialise identically.
        const bool hasToolBlocks = !msg.toolCalls.empty() || !msg.toolResults.empty();
        if (!hasToolBlocks) {
            messages.push_back({
                { "role", roleToString(msg.role) },
                { "content", msg.content.utf8_string() },
            });
            continue;
        }

        auto content = json::array();
        // Plain text leads the content array — matches Anthropic's
        // documented "assistant emits text then tool_use" pattern.
        if (!msg.content.empty()) {
            content.push_back({
                { "type", "text" },
                { "text", msg.content.utf8_string() },
            });
        }
        for (const auto& call : msg.toolCalls) {
            // Input must be a JSON object; if the captured arguments
            // string failed to parse, send `{}` and let the tool itself
            // surface the failure via its result. Better than dropping
            // the call entirely.
            auto input = json::parse(call.argumentsJson.utf8_string(), nullptr, false);
            if (input.is_discarded() || !input.is_object()) {
                input = json::object();
            }
            content.push_back({
                { "type", "tool_use" },
                { "id", call.id.utf8_string() },
                { "name", call.name.utf8_string() },
                { "input", std::move(input) },
            });
        }
        for (const auto& result : msg.toolResults) {
            json entry {
                { "type", "tool_result" },
                { "tool_use_id", result.toolUseId.utf8_string() },
                { "content", result.content.utf8_string() },
            };
            if (result.isError) {
                entry["is_error"] = true;
            }
            content.push_back(std::move(entry));
        }
        messages.push_back({
            { "role", roleToString(msg.role) },
            { "content", std::move(content) },
        });
    }
    body["messages"] = std::move(messages);

    // Tools array — present only when the host declared any. The
    // input schema travels as a raw JSON string per `AiTool`, so we
    // parse each one back to a json value for embedding. Schema
    // strings that fail to parse fall back to an empty object so the
    // tool still appears on the wire (the model may not exercise it,
    // but a malformed schema shouldn't take the whole request down).
    if (!request.tools.empty()) {
        auto tools = json::array();
        for (const auto& tool : request.tools) {
            auto schema = json::parse(tool.inputSchemaJson.utf8_string(), nullptr, false);
            if (schema.is_discarded()) {
                schema = json::object();
            }
            tools.push_back({
                { "name", tool.name.utf8_string() },
                { "description", tool.description.utf8_string() },
                { "input_schema", std::move(schema) },
            });
        }
        body["tools"] = std::move(tools);
    }
    return body.dump();
}

auto AnthropicProvider::buildBody(const AiRequest& request) const -> std::string {
    return serializeBody(request);
}

void AnthropicProvider::parseLine(const std::string_view line, StreamLineConsumer& sink) const {
    parseStreamLine(line, m_toolStates, sink);
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
