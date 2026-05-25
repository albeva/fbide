//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "WebStreamProvider.hpp"

namespace fbide::ai {

/**
 * AI provider backed by the Anthropic Messages API.
 *
 * Inherits `WebStreamProvider` for the HTTP + streaming + busy-state
 * scaffolding; only the URL, headers, body shape, and per-line parser
 * are provider-specific. Authentication uses the `x-api-key` header.
 *
 * **Threading:** UI thread only.
 */
class AnthropicProvider final : public WebStreamProvider {
public:
    NO_COPY_AND_MOVE(AnthropicProvider)

    /// Construct with the Anthropic API key (`x-api-key` header value).
    explicit AnthropicProvider(wxString apiKey);

    /// Map `AiRole` onto Anthropic's `messages[].role`. Exposed for tests.
    /// System folds to user — Anthropic carries the system prompt as a
    /// top-level field, so the System role never appears in `messages`.
    [[nodiscard]] static auto roleToString(AiRole role) -> const char*;

    /// Per-block state the SSE parser carries across calls when a
    /// `tool_use` content block streams its `input_json_delta`
    /// fragments. Keyed by `content_block` index inside the message.
    struct ToolUseState {
        wxString id;      ///< From `content_block_start.content_block.id`.
        wxString name;    ///< From `content_block_start.content_block.name`.
        std::string json; ///< Accumulated `input_json_delta.partial_json`.
    };
    using ToolUseStates = std::unordered_map<int, ToolUseState>;

    /// Parse one SSE line from Anthropic's `/v1/messages` stream into
    /// text deltas, tool-use events, and errors through `sink`.
    /// `states` accumulates per-block tool_use input JSON across calls
    /// — finalised `AiToolCall`s are emitted through `sink.onToolCall`
    /// on `content_block_stop`. `message_start` clears `states` so
    /// callers don't have to. Exposed as static so tests can drive
    /// canned line sequences without spinning up a provider.
    static void parseStreamLine(std::string_view line, ToolUseStates& states, StreamLineConsumer& sink);

    /// Stateless variant — drops tool-use events and only emits text
    /// deltas / errors. Preserved for tests written before tool use
    /// landed and for callers that don't care about tool events.
    static void parseStreamLine(std::string_view line, StreamLineConsumer& sink);

    /// Anthropic Messages API supports prompt caching with cache_control
    /// breakpoints on system content blocks.
    [[nodiscard]] auto supportsPromptCaching() const -> bool override { return true; }

    /// Anthropic Messages API supports tools via `tools` + `tool_use`
    /// / `tool_result` content blocks.
    [[nodiscard]] auto supportsTools() const -> bool override { return true; }

    /// Maximum number of cache_control breakpoints Anthropic accepts in
    /// one request — currently 4 across system + messages.
    static constexpr int kMaxCacheBreakpoints = 4;

    /// Static helper that serialises `request` into the JSON body string
    /// the API expects. The protected `buildBody` override delegates
    /// here; exposing the static lets tests assert the wire shape
    /// (system as string vs array, cache_control placement, tool_use /
    /// tool_result blocks, tools array) without constructing an
    /// `AnthropicProvider` instance or spinning up a real request.
    [[nodiscard]] static auto serializeBody(const AiRequest& request) -> std::string;

protected:
    [[nodiscard]] auto buildUrl(const AiRequest& request) const -> wxString override;
    void applyHeaders(wxWebRequest& request) const override;
    [[nodiscard]] auto buildBody(const AiRequest& request) const -> std::string override;
    void parseLine(std::string_view line, StreamLineConsumer& sink) const override;
    [[nodiscard]] auto httpErrorMessage(int status) const -> wxString override;
    [[nodiscard]] auto unauthorizedMessage() const -> wxString override;
    [[nodiscard]] auto requestFailedMessage(const wxString& detail) const -> wxString override;

private:
    wxString m_apiKey;                  ///< Anthropic API key.
    mutable ToolUseStates m_toolStates; ///< Per-block tool_use accumulator for the in-flight request.
};

} // namespace fbide::ai
