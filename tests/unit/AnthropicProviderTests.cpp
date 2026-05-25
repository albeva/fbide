//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/AnthropicProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Capture both deltas and the last error a parser emits.
/// Non-copyable / non-movable (inherits `StreamLineConsumer`'s deletes);
/// tests construct one on the stack and assert against it in place.
class CapturingSink final : public StreamLineConsumer {
public:
    void onDelta(const wxString& delta) override { deltas.push_back(delta); }
    void onError(const wxString& message) override { error = message; }

    std::vector<wxString> deltas;
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
