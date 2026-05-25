//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/OllamaProvider.hpp"
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

TEST(OllamaProvider, RoleMapsAllThreeDistinctly) {
    EXPECT_STREQ("assistant", OllamaProvider::roleToString(AiRole::Assistant));
    EXPECT_STREQ("user", OllamaProvider::roleToString(AiRole::User));
    EXPECT_STREQ("system", OllamaProvider::roleToString(AiRole::System));
}

// ---------------------------------------------------------------------------
// NDJSON parser
// ---------------------------------------------------------------------------

TEST(OllamaProvider, ParseEmitsContentFromMessage) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine(R"({"message":{"role":"assistant","content":"hello"},"done":false})", sink);
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hello", sink.deltas.at(0));
}

TEST(OllamaProvider, ParseIgnoresLinesWithoutMessage) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine(R"({"done":true,"total_duration":1234})", sink);
    EXPECT_TRUE(sink.deltas.empty());
    EXPECT_TRUE(sink.error.empty());
}

TEST(OllamaProvider, ParseIgnoresMalformedJson) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine("garbage", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(OllamaProvider, ParseSurfacesErrorString) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine(R"({"error":"model not found"})", sink);
    EXPECT_EQ("model not found", sink.error);
}

TEST(OllamaProvider, ParseEmptyMessageContentEmitsEmptyDelta) {
    // Empty content is still an event — the panel collapses many tiny
    // empty deltas anyway.
    CapturingSink sink;
    OllamaProvider::parseStreamLine(R"({"message":{"role":"assistant","content":""}})", sink);
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("", sink.deltas.at(0));
}

// ---------------------------------------------------------------------------
// Usage reporting — emitted on the terminal `done: true` chunk.
// ---------------------------------------------------------------------------

TEST(OllamaProvider, ParseEmitsUsageOnDoneChunk) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine(
        R"({"message":{"role":"assistant","content":""},"done":true,)"
        R"("prompt_eval_count":42,"eval_count":17})",
        sink
    );
    EXPECT_EQ(42, sink.usageInput);
    EXPECT_EQ(17, sink.usageOutput);
}

TEST(OllamaProvider, ParseSkipsUsageWhenCountsMissing) {
    CapturingSink sink;
    OllamaProvider::parseStreamLine(R"({"message":{"role":"assistant","content":"hi"}})", sink);
    EXPECT_EQ(0, sink.usageInput);
    EXPECT_EQ(0, sink.usageOutput);
}
