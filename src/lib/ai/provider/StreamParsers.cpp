//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "StreamParsers.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr std::string_view kSsePrefix = "data:";

/// Strip the `data:` prefix from an SSE payload line. Returns nullopt
/// when `line` doesn't start with the prefix.
auto stripSsePrefix(std::string_view line) -> std::optional<std::string_view> {
    if (!line.starts_with(kSsePrefix)) {
        return std::nullopt;
    }
    return line.substr(kSsePrefix.size());
}

/// Read the `error.message` string from a payload that carries either an
/// object (`{"message":"...", ...}`) or a non-object value. Falls back to
/// `Unknown API error.` so the user always gets a usable string.
auto extractErrorMessage(const json& error) -> wxString {
    if (error.is_object()) {
        return wxString::FromUTF8(error.value("message", "Unknown API error."));
    }
    return "Unknown API error.";
}

} // namespace

auto fbide::ai::anthropicRoleToString(const AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "assistant";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}

auto fbide::ai::ollamaRoleToString(const AiRole role) -> const char* {
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

auto fbide::ai::geminiRoleToString(const AiRole role) -> const char* {
    switch (role) {
    case AiRole::Assistant:
        return "model";
    case AiRole::User:
    case AiRole::System:
        break;
    }
    return "user";
}

void fbide::ai::parseAnthropicLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) {
    const auto payloadView = stripSsePrefix(line);
    if (!payloadView) {
        return;
    }
    const auto payload = json::parse(*payloadView, nullptr, false);
    if (payload.is_discarded()) {
        return;
    }
    const auto type = payload.value("type", "");
    if (type == "content_block_delta") {
        const auto& delta = payload["delta"];
        if (delta.is_object() && delta.value("type", "") == "text_delta") {
            onDelta(wxString::FromUTF8(delta.value("text", "")));
        }
    } else if (type == "error") {
        onError(extractErrorMessage(payload["error"]));
    }
}

void fbide::ai::parseOllamaLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) {
    const auto chunk = json::parse(line, nullptr, false);
    if (chunk.is_discarded()) {
        return;
    }
    if (chunk.contains("error")) {
        onError(wxString::FromUTF8(chunk.value("error", "Unknown Ollama error.")));
        return;
    }
    if (chunk.contains("message") && chunk["message"].is_object()) {
        onDelta(wxString::FromUTF8(chunk["message"].value("content", "")));
    }
}

void fbide::ai::parseGeminiLine(
    const std::string_view line,
    const StreamDeltaSink& onDelta,
    const StreamErrorSink& onError
) {
    const auto payloadView = stripSsePrefix(line);
    if (!payloadView) {
        return;
    }
    const auto payload = json::parse(*payloadView, nullptr, false);
    if (payload.is_discarded()) {
        return;
    }
    if (payload.contains("error")) {
        onError(extractErrorMessage(payload["error"]));
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
            onDelta(wxString::FromUTF8(part.value("text", "")));
        }
    }
}
