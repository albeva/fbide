//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/LmStudioProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

class CapturingSink final : public StreamLineConsumer {
public:
    void onDelta(const wxString& delta) override { deltas.push_back(delta); }
    void onError(const wxString& message) override { error = message; }
    void onUsage(int inputTokens, int outputTokens) override {
        usageInput = inputTokens;
        usageOutput = outputTokens;
    }

    std::vector<wxString> deltas;
    wxString error;
    int usageInput = 0;
    int usageOutput = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Role mapping
// ---------------------------------------------------------------------------

TEST(LmStudioProvider, RoleMapsAllThreeDistinctly) {
    EXPECT_STREQ("assistant", LmStudioProvider::roleToString(AiRole::Assistant));
    EXPECT_STREQ("user", LmStudioProvider::roleToString(AiRole::User));
    EXPECT_STREQ("system", LmStudioProvider::roleToString(AiRole::System));
}

// ---------------------------------------------------------------------------
// SSE parser — happy paths
// ---------------------------------------------------------------------------

TEST(LmStudioProvider, ParseEmitsContentFromDelta) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data: {"choices":[{"delta":{"content":"hello"}}]})",
        sink
    );
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hello", sink.deltas.at(0));
}

TEST(LmStudioProvider, ParseAcceptsNoSpaceAfterDataPrefix) {
    // RFC 8895 allows `data:foo` without the conventional space.
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data:{"choices":[{"delta":{"content":"hi"}}]})",
        sink
    );
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hi", sink.deltas.at(0));
}

TEST(LmStudioProvider, ParseIgnoresDoneSentinel) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine("data: [DONE]", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(LmStudioProvider, ParseIgnoresLinesWithoutDataPrefix) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(": keep-alive", sink);
    LmStudioProvider::parseStreamLine("event: chunk", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(LmStudioProvider, ParseIgnoresEmptyDataPayload) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine("data:", sink);
    LmStudioProvider::parseStreamLine("data: ", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(LmStudioProvider, ParseIgnoresMalformedJson) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine("data: garbage", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(LmStudioProvider, ParseIgnoresChunkWithoutChoices) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(R"(data: {"id":"chatcmpl-1","object":"chat.completion.chunk"})", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(LmStudioProvider, ParseIgnoresChunkWithoutDeltaContent) {
    // `finish_reason: "stop"` chunks carry an empty delta — no text to emit.
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data: {"choices":[{"delta":{},"finish_reason":"stop"}]})",
        sink
    );
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(LmStudioProvider, ParseSkipsEmptyContent) {
    // Some OpenAI-compatible servers send an opening chunk with
    // `delta.role` and no content; treat it as nothing to render.
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data: {"choices":[{"delta":{"role":"assistant","content":""}}]})",
        sink
    );
    EXPECT_TRUE(sink.deltas.empty());
}

// ---------------------------------------------------------------------------
// SSE parser — error paths
// ---------------------------------------------------------------------------

TEST(LmStudioProvider, ParseSurfacesErrorStringPayload) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(R"(data: {"error":"model not loaded"})", sink);
    EXPECT_EQ("model not loaded", sink.error);
}

TEST(LmStudioProvider, ParseSurfacesErrorObjectMessage) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(R"(data: {"error":{"message":"context window exceeded"}})", sink);
    EXPECT_EQ("context window exceeded", sink.error);
}

TEST(LmStudioProvider, ParseFallsBackOnEmptyErrorObject) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(R"(data: {"error":{}})", sink);
    EXPECT_EQ("Unknown LM Studio error.", sink.error);
}

// ---------------------------------------------------------------------------
// Usage reporting — emitted in the final chunk when stream_options.include_usage is on.
// ---------------------------------------------------------------------------

TEST(LmStudioProvider, ParseEmitsUsageWhenPresent) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data: {"choices":[],"usage":{"prompt_tokens":42,"completion_tokens":17,"total_tokens":59}})",
        sink
    );
    EXPECT_EQ(42, sink.usageInput);
    EXPECT_EQ(17, sink.usageOutput);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(LmStudioProvider, ParseSkipsUsageWhenAllZero) {
    CapturingSink sink;
    LmStudioProvider::parseStreamLine(
        R"(data: {"choices":[],"usage":{"prompt_tokens":0,"completion_tokens":0}})",
        sink
    );
    EXPECT_EQ(0, sink.usageInput);
    EXPECT_EQ(0, sink.usageOutput);
}
