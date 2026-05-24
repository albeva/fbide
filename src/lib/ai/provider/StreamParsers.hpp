//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ai/AiTypes.hpp"

namespace fbide::ai {

/// Streaming chunk handler — receives one reply text fragment.
using StreamDeltaSink = std::function<void(const wxString&)>;
/// Streaming error sink — receives one human-readable error message.
using StreamErrorSink = std::function<void(const wxString&)>;

/// Map `AiRole` onto Anthropic's `messages[].role`. System folds to user —
/// Anthropic carries the system prompt as a top-level field, so the System
/// role never appears in `messages`.
[[nodiscard]] auto anthropicRoleToString(AiRole role) -> const char*;

/// Map `AiRole` onto Ollama's `messages[].role`. Ollama carries the system
/// prompt as a `system`-role message in the array.
[[nodiscard]] auto ollamaRoleToString(AiRole role) -> const char*;

/// Map `AiRole` onto Gemini's `contents[].role`. Gemini uses `model` for
/// assistant and carries the system prompt as `systemInstruction`, so the
/// System role never appears here.
[[nodiscard]] auto geminiRoleToString(AiRole role) -> const char*;

/// Parse one SSE line from Anthropic's `/v1/messages` stream. Lines that
/// don't start with `data:` (events, blanks, comments) are ignored. A
/// `content_block_delta` with a `text_delta` payload invokes `onDelta`;
/// a `type:error` payload invokes `onError`. Malformed JSON is ignored.
void parseAnthropicLine(std::string_view line, const StreamDeltaSink& onDelta, const StreamErrorSink& onError);

/// Parse one NDJSON line from Ollama's `/api/chat` stream. A line carrying
/// `message.content` invokes `onDelta`; a line carrying `error` invokes
/// `onError`. Malformed JSON is ignored.
void parseOllamaLine(std::string_view line, const StreamDeltaSink& onDelta, const StreamErrorSink& onError);

/// Parse one SSE line from Gemini's `streamGenerateContent?alt=sse` stream.
/// `candidates[0].content.parts[*].text` emits one `onDelta` per part with
/// a `text` field; a top-level `error` invokes `onError`. Malformed JSON
/// is ignored.
void parseGeminiLine(std::string_view line, const StreamDeltaSink& onDelta, const StreamErrorSink& onError);

} // namespace fbide::ai
