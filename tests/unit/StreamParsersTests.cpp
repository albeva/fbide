//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/StreamParsers.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Capture both deltas and the last error a parser emits, in a single
/// place — every test wants both anyway.
struct Captured {
    std::vector<wxString> deltas;
    wxString error;
};

auto runParser(auto parser, const std::string_view line) -> Captured {
    Captured captured;
    parser(
        line,
        [&captured](const wxString& delta) { captured.deltas.push_back(delta); },
        [&captured](const wxString& message) { captured.error = message; }
    );
    return captured;
}

} // namespace

// ---------------------------------------------------------------------------
// Role mappings — every concrete role plus the System fall-through.
// ---------------------------------------------------------------------------

TEST(StreamParsers, AnthropicRoleToStringMapsAssistant) {
    EXPECT_STREQ("assistant", anthropicRoleToString(AiRole::Assistant));
}

TEST(StreamParsers, AnthropicRoleToStringMapsUser) {
    EXPECT_STREQ("user", anthropicRoleToString(AiRole::User));
}

TEST(StreamParsers, AnthropicRoleToStringMapsSystemAsUser) {
    // Anthropic carries the system prompt as a top-level field — the
    // System role should never appear in `messages`, so it folds to user.
    EXPECT_STREQ("user", anthropicRoleToString(AiRole::System));
}

TEST(StreamParsers, OllamaRoleToStringMapsAllThreeDistinctly) {
    EXPECT_STREQ("assistant", ollamaRoleToString(AiRole::Assistant));
    EXPECT_STREQ("user", ollamaRoleToString(AiRole::User));
    EXPECT_STREQ("system", ollamaRoleToString(AiRole::System));
}

TEST(StreamParsers, GeminiRoleToStringMapsAssistantAsModel) {
    // Gemini's protocol uses "model", not "assistant".
    EXPECT_STREQ("model", geminiRoleToString(AiRole::Assistant));
}

TEST(StreamParsers, GeminiRoleToStringMapsUser) {
    EXPECT_STREQ("user", geminiRoleToString(AiRole::User));
}

TEST(StreamParsers, GeminiRoleToStringMapsSystemAsUser) {
    // Gemini carries the system prompt as `systemInstruction` — System
    // never appears in `contents`.
    EXPECT_STREQ("user", geminiRoleToString(AiRole::System));
}

// ---------------------------------------------------------------------------
// Anthropic SSE parser.
// ---------------------------------------------------------------------------

TEST(StreamParsers, AnthropicEmitsDeltaFromContentBlockDelta) {
    const auto captured = runParser(parseAnthropicLine,
        R"(data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"hi"}})");
    ASSERT_EQ(1U, captured.deltas.size());
    EXPECT_EQ("hi", captured.deltas[0]);
    EXPECT_TRUE(captured.error.empty());
}

TEST(StreamParsers, AnthropicIgnoresNonDataLines) {
    EXPECT_TRUE(runParser(parseAnthropicLine, "event: message_start").deltas.empty());
    EXPECT_TRUE(runParser(parseAnthropicLine, "").deltas.empty());
    EXPECT_TRUE(runParser(parseAnthropicLine, ": comment").deltas.empty());
}

TEST(StreamParsers, AnthropicIgnoresMalformedJson) {
    EXPECT_TRUE(runParser(parseAnthropicLine, "data: {not json").deltas.empty());
}

TEST(StreamParsers, AnthropicIgnoresUnknownEventType) {
    const auto captured = runParser(parseAnthropicLine,
        R"(data: {"type":"message_start","message":{"id":"abc"}})");
    EXPECT_TRUE(captured.deltas.empty());
    EXPECT_TRUE(captured.error.empty());
}

TEST(StreamParsers, AnthropicIgnoresNonTextDelta) {
    // A `content_block_delta` with a non-text delta kind shouldn't emit.
    const auto captured = runParser(parseAnthropicLine,
        R"(data: {"type":"content_block_delta","delta":{"type":"input_json_delta","partial_json":"{}"}})");
    EXPECT_TRUE(captured.deltas.empty());
}

TEST(StreamParsers, AnthropicSurfacesErrorMessage) {
    const auto captured = runParser(parseAnthropicLine,
        R"(data: {"type":"error","error":{"type":"overloaded_error","message":"server busy"}})");
    EXPECT_EQ("server busy", captured.error);
    EXPECT_TRUE(captured.deltas.empty());
}

TEST(StreamParsers, AnthropicErrorFallsBackToDefaultWhenMessageMissing) {
    const auto captured = runParser(parseAnthropicLine,
        R"(data: {"type":"error","error":"oops"})");
    EXPECT_EQ("Unknown API error.", captured.error);
}

// ---------------------------------------------------------------------------
// Ollama NDJSON parser.
// ---------------------------------------------------------------------------

TEST(StreamParsers, OllamaEmitsContentFromMessage) {
    const auto captured = runParser(parseOllamaLine,
        R"({"message":{"role":"assistant","content":"hello"},"done":false})");
    ASSERT_EQ(1U, captured.deltas.size());
    EXPECT_EQ("hello", captured.deltas[0]);
}

TEST(StreamParsers, OllamaIgnoresLinesWithoutMessage) {
    const auto captured = runParser(parseOllamaLine, R"({"done":true,"total_duration":1234})");
    EXPECT_TRUE(captured.deltas.empty());
    EXPECT_TRUE(captured.error.empty());
}

TEST(StreamParsers, OllamaIgnoresMalformedJson) {
    EXPECT_TRUE(runParser(parseOllamaLine, "garbage").deltas.empty());
}

TEST(StreamParsers, OllamaSurfacesErrorString) {
    const auto captured = runParser(parseOllamaLine, R"({"error":"model not found"})");
    EXPECT_EQ("model not found", captured.error);
}

TEST(StreamParsers, OllamaEmptyMessageContentEmitsEmptyDelta) {
    // Empty content is still an event — the panel collapses many tiny
    // empty deltas anyway.
    const auto captured = runParser(parseOllamaLine,
        R"({"message":{"role":"assistant","content":""}})");
    ASSERT_EQ(1U, captured.deltas.size());
    EXPECT_EQ("", captured.deltas[0]);
}

// ---------------------------------------------------------------------------
// Gemini SSE parser.
// ---------------------------------------------------------------------------

TEST(StreamParsers, GeminiEmitsTextFromFirstCandidate) {
    const auto captured = runParser(parseGeminiLine,
        R"(data: {"candidates":[{"content":{"role":"model","parts":[{"text":"hi"}]}}]})");
    ASSERT_EQ(1U, captured.deltas.size());
    EXPECT_EQ("hi", captured.deltas[0]);
}

TEST(StreamParsers, GeminiEmitsOneDeltaPerPart) {
    const auto captured = runParser(parseGeminiLine,
        R"(data: {"candidates":[{"content":{"parts":[{"text":"a"},{"text":"b"}]}}]})");
    ASSERT_EQ(2U, captured.deltas.size());
    EXPECT_EQ("a", captured.deltas[0]);
    EXPECT_EQ("b", captured.deltas[1]);
}

TEST(StreamParsers, GeminiIgnoresNonDataLines) {
    EXPECT_TRUE(runParser(parseGeminiLine, "").deltas.empty());
    EXPECT_TRUE(runParser(parseGeminiLine, "event: keep-alive").deltas.empty());
}

TEST(StreamParsers, GeminiIgnoresMalformedJson) {
    EXPECT_TRUE(runParser(parseGeminiLine, "data: {borked").deltas.empty());
}

TEST(StreamParsers, GeminiIgnoresEmptyCandidatesArray) {
    EXPECT_TRUE(runParser(parseGeminiLine, R"(data: {"candidates":[]})").deltas.empty());
}

TEST(StreamParsers, GeminiSurfacesErrorMessage) {
    const auto captured = runParser(parseGeminiLine,
        R"(data: {"error":{"code":400,"message":"bad request"}})");
    EXPECT_EQ("bad request", captured.error);
    EXPECT_TRUE(captured.deltas.empty());
}

TEST(StreamParsers, GeminiErrorFallsBackToDefaultWhenMessageMissing) {
    const auto captured = runParser(parseGeminiLine, R"(data: {"error":"oops"})");
    EXPECT_EQ("Unknown API error.", captured.error);
}

TEST(StreamParsers, GeminiSkipsPartsWithoutText) {
    // A part may carry only metadata — skip those parts but still emit
    // any sibling parts that DO carry text.
    const auto captured = runParser(parseGeminiLine,
        R"(data: {"candidates":[{"content":{"parts":[{"functionCall":{}},{"text":"after"}]}}]})");
    ASSERT_EQ(1U, captured.deltas.size());
    EXPECT_EQ("after", captured.deltas[0]);
}
