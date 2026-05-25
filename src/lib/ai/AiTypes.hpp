//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/// Role of a message within an AI conversation.
enum class AiRole : std::uint8_t {
    System,
    User,
    Assistant
};

/// One model-invocable tool exposed in a request. Tools are declared by
/// the host, the model returns a `tool_use` block, the host executes the
/// tool and feeds the result back as a `tool_result` block in the next
/// turn. `inputSchemaJson` is the raw JSON-schema string that constrains
/// the model's `argumentsJson` — providers embed it verbatim.
struct AiTool {
    wxString name;            ///< Identifier the model invokes by.
    wxString description;     ///< Model-visible purpose / usage hint.
    wxString inputSchemaJson; ///< JSON-schema describing arguments.
};

/// One tool call requested by the model in an assistant message. The
/// host runs the named tool with `argumentsJson` (raw JSON) and replies
/// with an `AiToolResult` carrying the same `id` in `toolUseId`.
struct AiToolCall {
    wxString id;            ///< Provider-issued correlation id (round-trips in the result).
    wxString name;          ///< Tool name from the registry.
    wxString argumentsJson; ///< Raw JSON arguments object.
};

/// One tool result the host sends back to the model after running a
/// tool. `content` is the text the model sees; `isError = true` lets
/// the model retry intelligently (e.g. ask the user to pin a file).
struct AiToolResult {
    wxString toolUseId;   ///< Echoes the originating `AiToolCall::id`.
    wxString content;     ///< Result text presented to the model.
    bool isError = false; ///< True when the tool failed — model gets a structured failure cue.
};

/// One message in an AI conversation. `toolCalls` is populated on
/// assistant messages that include `tool_use` blocks; `toolResults` on
/// user messages that respond to them. Both stay empty for normal
/// chat turns.
struct AiMessage {
    AiRole role;                           ///< Who authored the message.
    wxString content;                      ///< Message text (may be empty if only tool blocks are present).
    std::vector<AiToolCall> toolCalls;     ///< Assistant-side: tool_use blocks the model emitted.
    std::vector<AiToolResult> toolResults; ///< User-side: tool_result blocks the host fed back.
};

/// One block of the system prompt. The `cacheable` flag is a hint to
/// providers that support prompt caching (currently only Anthropic) —
/// stable content (base prompt, file context) is marked true so the
/// provider can attach a cache breakpoint; volatile content (open-tab
/// buffer snapshots) stays false. Providers without caching ignore the
/// flag and concatenate via `joinSystem`.
struct AiContent {
    wxString text;
    bool cacheable = false;
};

/// Concatenate `blocks` with `"\n\n"` separators, skipping empty
/// blocks. Used by providers that lack prompt caching to collapse the
/// structured form back to a single string.
[[nodiscard]] auto joinSystem(const std::vector<AiContent>& blocks) -> wxString;

/// Default cap on assistant reply length. Providers map this onto their
/// `max_tokens` (or equivalent) request field.
inline constexpr int kDefaultMaxTokens = 1024;

/// Provider-neutral request: a model name plus the conversation so far.
/// Carries no vendor-specific fields — each `AiProvider` maps this onto
/// its own wire format.
struct AiRequest {
    wxString model;                    ///< Model identifier.
    std::vector<AiContent> system;     ///< Optional system prompt, one block per
                                       ///< section. Providers that support prompt
                                       ///< caching emit a cache breakpoint after
                                       ///< each cacheable block; the rest fold
                                       ///< the vector back to a string via
                                       ///< `joinSystem`.
    std::vector<AiMessage> messages;   ///< Conversation, oldest first.
    std::vector<AiTool> tools;         ///< Tools exposed to the model. Empty when
                                       ///< tools are disabled or the active
                                       ///< provider does not support them.
    int maxTokens = kDefaultMaxTokens; ///< Reply length cap.
};

/// Provider-neutral response.
struct AiResponse {
    bool ok = false; ///< True when `text` holds a valid reply.
    wxString text;   ///< Assistant reply (set when `ok`).
    wxString error;  ///< Human-readable failure reason (set when not `ok`).
};

} // namespace fbide::ai
