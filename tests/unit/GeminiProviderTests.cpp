//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/GeminiProvider.hpp"
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

TEST(GeminiProvider, RoleAssistantMapsToModel) {
    // Gemini's protocol uses "model", not "assistant".
    EXPECT_STREQ("model", GeminiProvider::roleToString(AiRole::Assistant));
}

TEST(GeminiProvider, RoleUserMapsToUser) {
    EXPECT_STREQ("user", GeminiProvider::roleToString(AiRole::User));
}

TEST(GeminiProvider, RoleSystemFoldsToUser) {
    // Gemini carries the system prompt as `systemInstruction` — System
    // never appears in `contents`.
    EXPECT_STREQ("user", GeminiProvider::roleToString(AiRole::System));
}

// ---------------------------------------------------------------------------
// SSE parser
// ---------------------------------------------------------------------------

TEST(GeminiProvider, ParseEmitsTextFromFirstCandidate) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(
        R"(data: {"candidates":[{"content":{"role":"model","parts":[{"text":"hi"}]}}]})",
        sink
    );
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hi", sink.deltas.at(0));
}

TEST(GeminiProvider, ParseEmitsOneDeltaPerPart) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(
        R"(data: {"candidates":[{"content":{"parts":[{"text":"a"},{"text":"b"}]}}]})",
        sink
    );
    ASSERT_EQ(2U, sink.deltas.size());
    EXPECT_EQ("a", sink.deltas.at(0));
    EXPECT_EQ("b", sink.deltas.at(1));
}

TEST(GeminiProvider, ParseIgnoresNonDataLines) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine("", sink);
    GeminiProvider::parseStreamLine("event: keep-alive", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(GeminiProvider, ParseIgnoresMalformedJson) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine("data: {borked", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(GeminiProvider, ParseIgnoresEmptyCandidatesArray) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(R"(data: {"candidates":[]})", sink);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(GeminiProvider, ParseSurfacesErrorMessage) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(R"(data: {"error":{"code":400,"message":"bad request"}})", sink);
    EXPECT_EQ("bad request", sink.error);
    EXPECT_TRUE(sink.deltas.empty());
}

TEST(GeminiProvider, ParseErrorFallsBackToDefaultWhenMessageMissing) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(R"(data: {"error":"oops"})", sink);
    EXPECT_EQ("Unknown API error.", sink.error);
}

TEST(GeminiProvider, ParseSkipsPartsWithoutText) {
    // A part may carry only metadata — skip those parts but still emit
    // any sibling parts that DO carry text.
    CapturingSink sink;
    GeminiProvider::parseStreamLine(
        R"(data: {"candidates":[{"content":{"parts":[{"functionCall":{}},{"text":"after"}]}}]})",
        sink
    );
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("after", sink.deltas.at(0));
}

// ---------------------------------------------------------------------------
// Usage reporting — Gemini sends `usageMetadata` on streamed chunks.
// ---------------------------------------------------------------------------

TEST(GeminiProvider, ParseEmitsUsageFromUsageMetadata) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(
        R"(data: {"candidates":[{"content":{"parts":[{"text":"hi"}]}}],)"
        R"("usageMetadata":{"promptTokenCount":12,"candidatesTokenCount":4,"totalTokenCount":16}})",
        sink
    );
    EXPECT_EQ(12, sink.usageInput);
    EXPECT_EQ(4, sink.usageOutput);
    ASSERT_EQ(1U, sink.deltas.size());
    EXPECT_EQ("hi", sink.deltas.at(0));
}

TEST(GeminiProvider, ParseSkipsUsageWhenAllZero) {
    CapturingSink sink;
    GeminiProvider::parseStreamLine(
        R"(data: {"usageMetadata":{"promptTokenCount":0,"candidatesTokenCount":0}})",
        sink
    );
    EXPECT_EQ(0, sink.usageInput);
    EXPECT_EQ(0, sink.usageOutput);
}
