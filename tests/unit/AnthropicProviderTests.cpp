//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/AnthropicProvider.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

/// Capture both deltas, tool calls, and the last error a parser emits.
/// Non-copyable / non-movable (inherits `StreamLineConsumer`'s deletes);
/// tests construct one on the stack and assert against it in place.
class CapturingSink final : public StreamLineConsumer {
public:
    void onDelta(const wxString& delta) override { deltas.push_back(delta); }
    void onError(const wxString& message) override { error = message; }
    void onToolCall(AiToolCall call) override { toolCalls.push_back(std::move(call)); }

    std::vector<wxString> deltas;
    std::vector<AiToolCall> toolCalls;
    wxString error;
};

} // namespace

// ---------------------------------------------------------------------------
// Role mapping
// ---------------------------------------------------------------------------

TEST(AnthropicProvider, RoleAssistantMapsToAssistant) {
    EXPECT_STREQ("assistant", AnthropicProvider::roleToString(AiRole::Assistant));
}

TEST(AnthropicProvider, RoleUserMapsToUser) {
    EXPECT_STREQ("user", AnthropicProvider::roleToString(AiRole::User));
}

TEST(AnthropicProvider, RoleSystemFoldsToUser) {
    // Anthropic carries the system prompt as a top-level field — the
    // System role should never appear in `messages`, so it folds to user.
    EXPECT_STREQ("user", AnthropicProvider::roleToString(AiRole::System));
}

// ---------------------------------------------------------------------------
// SSE parser
// ---------------------------------------------------------------------------

TEST(AnthropicProvider, ParseEmitsDeltaFromContentBlockDelta) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"hi"}})",
        sink
    );
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hi", sink.deltas.at(0));
    EXPECT_TRUE(sink.error.empty());
}

TEST(AnthropicProvider, ParseIgnoresNonDataLines) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine("event: message_start", sink);
    AnthropicProvider::parseStreamLine("", sink);
    AnthropicProvider::parseStreamLine(": comment", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(AnthropicProvider, ParseIgnoresMalformedJson) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine("data: {not json", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(AnthropicProvider, ParseIgnoresUnknownEventType) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine(R"(data: {"type":"message_start","message":{"id":"abc"}})", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(AnthropicProvider, ParseIgnoresNonTextDelta) {
    // A `content_block_delta` with a non-text delta kind shouldn't emit.
    CapturingSink sink;
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","delta":{"type":"input_json_delta","partial_json":"{}"}})",
        sink
    );
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(AnthropicProvider, ParseSurfacesErrorMessage) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"error","error":{"type":"overloaded_error","message":"server busy"}})",
        sink
    );
    EXPECT_EQ("server busy", sink.error);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(AnthropicProvider, ParseErrorFallsBackToDefaultWhenMessageMissing) {
    CapturingSink sink;
    AnthropicProvider::parseStreamLine(R"(data: {"type":"error","error":"oops"})", sink);
    EXPECT_EQ("Unknown API error.", sink.error);
}

// ---------------------------------------------------------------------------
// buildBody — system serialisation + prompt caching
// ---------------------------------------------------------------------------

TEST(AnthropicProvider, AdvertisesPromptCachingSupport) {
    const AnthropicProvider provider("k");
    EXPECT_TRUE(provider.supportsPromptCaching());
}

TEST(AnthropicProviderBuildBody, OmitsSystemWhenAllBlocksEmpty) {
    const AnthropicProvider provider("k");
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .system = {},
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    EXPECT_FALSE(body.contains("system"));
}

TEST(AnthropicProviderBuildBody, EmitsStringFormWhenNoBlocksAreCacheable) {
    const AnthropicProvider provider("k");
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .system = {
            { .text = "alpha", .cacheable = false },
            { .text = "beta", .cacheable = false },
        },
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body["system"].is_string());
    EXPECT_EQ("alpha\n\nbeta", body["system"].get<std::string>());
}

TEST(AnthropicProviderBuildBody, EmitsArrayFormWithCacheControlWhenAnyBlockIsCacheable) {
    const AnthropicProvider provider("k");
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .system = {
            { .text = "base prompt", .cacheable = true },
            { .text = "buffer snapshot", .cacheable = false },
        },
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body["system"].is_array());
    ASSERT_EQ(2U, body["system"].size());

    EXPECT_EQ("text", body["system"][0]["type"].get<std::string>());
    EXPECT_EQ("base prompt", body["system"][0]["text"].get<std::string>());
    ASSERT_TRUE(body["system"][0].contains("cache_control"));
    EXPECT_EQ("ephemeral", body["system"][0]["cache_control"]["type"].get<std::string>());

    EXPECT_EQ("buffer snapshot", body["system"][1]["text"].get<std::string>());
    EXPECT_FALSE(body["system"][1].contains("cache_control"));
}

TEST(AnthropicProviderBuildBody, SkipsEmptyBlocksInArrayForm) {
    const AnthropicProvider provider("k");
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .system = {
            { .text = "first", .cacheable = true },
            { .text = "", .cacheable = true },
            { .text = "third", .cacheable = false },
        },
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body["system"].is_array());
    ASSERT_EQ(2U, body["system"].size());
    EXPECT_EQ("first", body["system"][0]["text"].get<std::string>());
    EXPECT_EQ("third", body["system"][1]["text"].get<std::string>());
}

// ---------------------------------------------------------------------------
// buildBody — tools, tool_use, tool_result
// ---------------------------------------------------------------------------

TEST(AnthropicProvider, AdvertisesToolSupport) {
    const AnthropicProvider provider("k");
    EXPECT_TRUE(provider.supportsTools());
}

TEST(AnthropicProviderBuildBody, OmitsToolsWhenRequestHasNone) {
    const AiRequest request {
        .model = "claude-sonnet-4-6",
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    EXPECT_FALSE(body.contains("tools"));
}

TEST(AnthropicProviderBuildBody, EmitsToolsArrayWithParsedSchema) {
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .messages = { { .role = AiRole::User, .content = "hi" } },
        .tools = {
            AiTool {
                .name = "read_file",
                .description = "read",
                .inputSchemaJson = R"({"type":"object","properties":{"path":{"type":"string"}}})",
            },
        },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body.contains("tools"));
    ASSERT_EQ(1U, body["tools"].size());
    EXPECT_EQ("read_file", body["tools"][0]["name"].get<std::string>());
    EXPECT_EQ("read", body["tools"][0]["description"].get<std::string>());
    // input_schema is the parsed JSON, not the string.
    EXPECT_EQ("object", body["tools"][0]["input_schema"]["type"].get<std::string>());
}

TEST(AnthropicProviderBuildBody, EmitsToolUseBlocksInAssistantContentArray) {
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .messages = {
            { .role = AiRole::User, .content = "what's in foo.bas?" },
            { .role = AiRole::Assistant,
                .content = "let me check",
                .toolCalls = {
                    AiToolCall {
                        .id = "tool-abc",
                        .name = "read_file",
                        .argumentsJson = R"({"path":"foo.bas"})",
                    },
                } },
        },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_EQ(2U, body["messages"].size());
    // Text-only user message keeps the string content form.
    EXPECT_TRUE(body["messages"][0]["content"].is_string());
    // Assistant with tool_use uses the content-array form.
    ASSERT_TRUE(body["messages"][1]["content"].is_array());
    ASSERT_EQ(2U, body["messages"][1]["content"].size());
    EXPECT_EQ("text", body["messages"][1]["content"][0]["type"].get<std::string>());
    EXPECT_EQ("let me check", body["messages"][1]["content"][0]["text"].get<std::string>());
    EXPECT_EQ("tool_use", body["messages"][1]["content"][1]["type"].get<std::string>());
    EXPECT_EQ("tool-abc", body["messages"][1]["content"][1]["id"].get<std::string>());
    EXPECT_EQ("read_file", body["messages"][1]["content"][1]["name"].get<std::string>());
    EXPECT_EQ("foo.bas", body["messages"][1]["content"][1]["input"]["path"].get<std::string>());
}

TEST(AnthropicProviderBuildBody, EmitsToolResultBlocksInUserContentArray) {
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .messages = {
            { .role = AiRole::User,
                .content = {},
                .toolResults = {
                    AiToolResult { .toolUseId = "tool-abc", .content = "file body", .isError = false },
                    AiToolResult { .toolUseId = "tool-xyz", .content = "not found", .isError = true },
                } },
        },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body["messages"][0]["content"].is_array());
    ASSERT_EQ(2U, body["messages"][0]["content"].size());

    EXPECT_EQ("tool_result", body["messages"][0]["content"][0]["type"].get<std::string>());
    EXPECT_EQ("tool-abc", body["messages"][0]["content"][0]["tool_use_id"].get<std::string>());
    EXPECT_EQ("file body", body["messages"][0]["content"][0]["content"].get<std::string>());
    EXPECT_FALSE(body["messages"][0]["content"][0].contains("is_error"));

    EXPECT_TRUE(body["messages"][0]["content"][1]["is_error"].get<bool>());
}

TEST(AnthropicProviderBuildBody, FallsBackToEmptyObjectOnInvalidArgumentsJson) {
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .messages = {
            { .role = AiRole::Assistant,
                .content = {},
                .toolCalls = {
                    AiToolCall { .id = "x", .name = "y", .argumentsJson = "not-json" },
                } },
        },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    // Garbage arguments degrade to an empty object so the tool itself
    // can surface a structured "missing field" error.
    EXPECT_TRUE(body["messages"][0]["content"][0]["input"].is_object());
    EXPECT_TRUE(body["messages"][0]["content"][0]["input"].empty());
}

// ---------------------------------------------------------------------------
// Stream parser — tool_use state machine
// ---------------------------------------------------------------------------

TEST(AnthropicProviderToolUse, EmitsToolCallAcrossSplitJsonDeltas) {
    CapturingSink sink;
    AnthropicProvider::ToolUseStates states;

    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"message_start"})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_start","index":0,"content_block":{"type":"tool_use","id":"call-1","name":"read_file"}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","index":0,"delta":{"type":"input_json_delta","partial_json":"{\"path\":"}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","index":0,"delta":{"type":"input_json_delta","partial_json":"\"foo.bas\"}"}})",
        states, sink
    );
    // No tool call emitted yet — assembler waits for content_block_stop.
    EXPECT_TRUE(sink.toolCalls.empty());

    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_stop","index":0})",
        states, sink
    );
    ASSERT_EQ(1U, sink.toolCalls.size());
    EXPECT_EQ("call-1", sink.toolCalls.at(0).id);
    EXPECT_EQ("read_file", sink.toolCalls.at(0).name);
    EXPECT_EQ(R"({"path":"foo.bas"})", sink.toolCalls.at(0).argumentsJson);
    // State is consumed on stop so the same index can be reused next message.
    EXPECT_TRUE(states.empty());
}

TEST(AnthropicProviderToolUse, InterleavedTextAndToolUseBlocksBothEmit) {
    CapturingSink sink;
    AnthropicProvider::ToolUseStates states;

    AnthropicProvider::parseStreamLine(R"(data: {"type":"message_start"})", states, sink);
    // Block 0 — text.
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"thinking..."}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(R"(data: {"type":"content_block_stop","index":0})", states, sink);
    // Block 1 — tool_use.
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"c","name":"read_file"}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"{}"}})",
        states, sink
    );
    AnthropicProvider::parseStreamLine(R"(data: {"type":"content_block_stop","index":1})", states, sink);

    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("thinking...", sink.deltas.at(0));
    ASSERT_EQ(1U, sink.toolCalls.size());
    EXPECT_EQ("read_file", sink.toolCalls.at(0).name);
}

TEST(AnthropicProviderToolUse, MessageStartClearsDanglingState) {
    CapturingSink sink;
    AnthropicProvider::ToolUseStates states;

    // Pre-seed state to simulate a partial tool_use from a previous
    // request that never reached content_block_stop.
    AnthropicProvider::parseStreamLine(
        R"(data: {"type":"content_block_start","index":0,"content_block":{"type":"tool_use","id":"stale","name":"x"}})",
        states, sink
    );
    EXPECT_EQ(1U, states.size());

    AnthropicProvider::parseStreamLine(R"(data: {"type":"message_start"})", states, sink);
    EXPECT_TRUE(states.empty());
}

TEST(AnthropicProviderBuildBody, CapsCacheControlMarkersAtFour) {
    const AnthropicProvider provider("k");
    AiRequest request {
        .model = "claude-sonnet-4-6",
        .system = {
            { .text = "one", .cacheable = true },
            { .text = "two", .cacheable = true },
            { .text = "three", .cacheable = true },
            { .text = "four", .cacheable = true },
            { .text = "five", .cacheable = true },
        },
        .messages = { { .role = AiRole::User, .content = "hi" } },
    };
    const auto body = json::parse(AnthropicProvider::serializeBody(request));
    ASSERT_TRUE(body["system"].is_array());
    ASSERT_EQ(5U, body["system"].size());
    int markers = 0;
    for (const auto& entry : body["system"]) {
        if (entry.contains("cache_control")) {
            ++markers;
        }
    }
    EXPECT_EQ(AnthropicProvider::kMaxCacheBreakpoints, markers);
    // First four get markers; the fifth does not.
    EXPECT_TRUE(body["system"][0].contains("cache_control"));
    EXPECT_TRUE(body["system"][3].contains("cache_control"));
    EXPECT_FALSE(body["system"][4].contains("cache_control"));
}
