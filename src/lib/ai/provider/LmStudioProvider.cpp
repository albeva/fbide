//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "LmStudioProvider.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr std::string_view kSsePrefix = "data:";
constexpr std::string_view kSseDone = "[DONE]";

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

/// Read an integer at `key` from `node`. Returns 0 when absent or the
/// value isn't an integer-coercible number.
auto borrowInt(const json& node, const char* key) -> int {
    const auto it = node.find(key);
    if (it == node.end() || !it->is_number_integer()) {
        return 0;
    }
    return it->get<int>();
}

/// Drop SSE framing whitespace — RFC 8895 allows `data:foo` and
/// `data: foo`; we accept either.
auto stripLeadingSpace(std::string_view value) -> std::string_view {
    while (!value.empty() && value.front() == ' ') {
        value.remove_prefix(1);
    }
    return value;
}

} // namespace

LmStudioProvider::LmStudioProvider(wxString endpoint, wxString apiKey)
: m_endpoint(trimTrailingSlash(std::move(endpoint)))
, m_apiKey(std::move(apiKey)) {}

auto LmStudioProvider::roleToString(const AiRole role) -> const char* {
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

void LmStudioProvider::parseStreamLine(const std::string_view line, StreamLineConsumer& sink) {
    if (!line.starts_with(kSsePrefix)) {
        return;
    }
    const auto payload = stripLeadingSpace(line.substr(kSsePrefix.size()));
    if (payload.empty() || payload == kSseDone) {
        return;
    }
    const auto chunk = json::parse(payload, nullptr, false);
    if (chunk.is_discarded()) {
        return;
    }
    if (const auto errorIt = chunk.find("error"); errorIt != chunk.end()) {
        // OpenAI-style payload can be either a string or `{message: "..."}`.
        if (errorIt->is_object()) {
            const auto message = borrowString(*errorIt, "message");
            sink.onError(message.empty()
                             ? wxString { "Unknown LM Studio error." }
                             : wxString::FromUTF8(message.data(), message.size()));
        } else if (errorIt->is_string()) {
            const auto& message = errorIt->get_ref<const json::string_t&>();
            sink.onError(wxString::FromUTF8(message.data(), message.size()));
        } else {
            sink.onError("Unknown LM Studio error.");
        }
        return;
    }
    // `usage` arrives in a final chunk when the client opts in via
    // `stream_options.include_usage`. The choices array is empty on
    // that chunk, so handle usage first and let the no-delta path
    // below skip it.
    if (const auto usageIt = chunk.find("usage");
        usageIt != chunk.end() && usageIt->is_object()) {
        const auto prompt = borrowInt(*usageIt, "prompt_tokens");
        const auto completion = borrowInt(*usageIt, "completion_tokens");
        if (prompt > 0 || completion > 0) {
            sink.onUsage(prompt, completion);
        }
    }
    const auto choicesIt = chunk.find("choices");
    if (choicesIt == chunk.end() || !choicesIt->is_array() || choicesIt->empty()) {
        return;
    }
    const auto& choice = choicesIt->front();
    const auto deltaIt = choice.find("delta");
    if (deltaIt == choice.end() || !deltaIt->is_object()) {
        return;
    }
    const auto content = borrowString(*deltaIt, "content");
    if (content.empty()) {
        return;
    }
    sink.onDelta(wxString::FromUTF8(content.data(), content.size()));
}

auto LmStudioProvider::buildUrl(const AiRequest& /*request*/) const -> wxString {
    return m_endpoint + "/v1/chat/completions";
}

void LmStudioProvider::applyHeaders(wxWebRequest& request) const {
    // LM Studio does not require authentication by default, but a user
    // running it behind a reverse proxy may add one — forward the
    // configured key when present.
    if (!m_apiKey.empty()) {
        request.SetHeader("Authorization", "Bearer " + m_apiKey);
    }
}

auto LmStudioProvider::buildBody(const AiRequest& request) const -> std::string {
    json body;
    body["model"] = request.model.utf8_string();
    body["stream"] = true;
    body["max_tokens"] = request.maxTokens;
    // Opt into usage on the final chunk so we can report per-turn token
    // counts in the UI footer (matches what Anthropic emits natively).
    body["stream_options"] = { { "include_usage", true } };
    auto messages = json::array();
    if (const auto system = joinSystem(request.system); !system.empty()) {
        messages.push_back({
            { "role", "system" },
            { "content", system.utf8_string() },
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

void LmStudioProvider::parseLine(const std::string_view line, StreamLineConsumer& sink) const {
    parseStreamLine(line, sink);
}

auto LmStudioProvider::httpErrorMessage(const int status) const -> wxString {
    return wxString::Format("LM Studio error (HTTP %d).", status);
}

auto LmStudioProvider::unauthorizedMessage() const -> wxString {
    return "LM Studio returned Unauthorized — set `key` in the config if the server requires a bearer token.";
}

auto LmStudioProvider::requestFailedMessage(const wxString& detail) const -> wxString {
    // Most failures are "no server listening" — surface the hint so the
    // user doesn't have to guess which side of the wire is wrong.
    return "Request failed: " + detail + " (is the LM Studio server running?)";
}
