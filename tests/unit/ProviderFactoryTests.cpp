//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/ProviderFactory.hpp"
#include "ai/provider/AiProvider.hpp"
#include "ai/provider/AnthropicProvider.hpp"
#include "ai/provider/ClaudeCliProvider.hpp"
#include "ai/provider/GeminiProvider.hpp"
#include "ai/provider/LmStudioProvider.hpp"
#include "ai/provider/MockProvider.hpp"
#include "ai/provider/OllamaProvider.hpp"
#include "config/Value.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// `dynamic_cast` shorthand — every test wants the same "is this the
/// concrete provider I expect?" check.
template<typename T>
auto isInstanceOf(const AiProvider* provider) -> bool {
    return dynamic_cast<const T*>(provider) != nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Each supported `kind` builds the matching concrete provider.
// ---------------------------------------------------------------------------

TEST(MakeProvider, OllamaProducesOllamaProvider) {
    const Value config;
    const auto selection = makeProvider("ollama", config);
    EXPECT_TRUE(isInstanceOf<OllamaProvider>(selection.provider.get()));
}

TEST(MakeProvider, LmStudioProducesLmStudioProvider) {
    const Value config;
    const auto selection = makeProvider("lm-studio", config);
    EXPECT_TRUE(isInstanceOf<LmStudioProvider>(selection.provider.get()));
}

TEST(MakeProvider, ClaudeCliProducesClaudeCliProvider) {
    const Value config;
    const auto selection = makeProvider("claude-cli", config);
    EXPECT_TRUE(isInstanceOf<ClaudeCliProvider>(selection.provider.get()));
}

TEST(MakeProvider, MockProducesMockProvider) {
    const Value config;
    const auto selection = makeProvider("mock", config);
    EXPECT_TRUE(isInstanceOf<MockProvider>(selection.provider.get()));
    // Mock carries no model and needs no key.
    EXPECT_TRUE(selection.model.empty());
}

TEST(MakeProvider, AnthropicWithKeyProducesAnthropicProvider) {
    Value config;
    config["key"] = "test-anthropic-key";
    const auto selection = makeProvider("anthropic", config);
    EXPECT_TRUE(isInstanceOf<AnthropicProvider>(selection.provider.get()));
}

TEST(MakeProvider, GeminiWithKeyProducesGeminiProvider) {
    Value config;
    config["key"] = "test-gemini-key";
    const auto selection = makeProvider("gemini", config);
    EXPECT_TRUE(isInstanceOf<GeminiProvider>(selection.provider.get()));
}

// ---------------------------------------------------------------------------
// Missing-credential paths — provider is null, model is still resolved
// so the UI can surface "no key" rather than "no provider known".
// ---------------------------------------------------------------------------

TEST(MakeProvider, AnthropicWithoutKeyReturnsNullProvider) {
    const Value config;
    const auto selection = makeProvider("anthropic", config);
    EXPECT_EQ(nullptr, selection.provider.get());
    EXPECT_FALSE(selection.model.empty()); // default still applied
}

TEST(MakeProvider, AnthropicWithEmptyKeyReturnsNullProvider) {
    Value config;
    config["key"] = "";
    const auto selection = makeProvider("anthropic", config);
    EXPECT_EQ(nullptr, selection.provider.get());
}

TEST(MakeProvider, GeminiWithoutKeyReturnsNullProvider) {
    const Value config;
    const auto selection = makeProvider("gemini", config);
    EXPECT_EQ(nullptr, selection.provider.get());
    EXPECT_FALSE(selection.model.empty());
}

TEST(MakeProvider, GeminiWithEmptyKeyReturnsNullProvider) {
    Value config;
    config["key"] = "";
    const auto selection = makeProvider("gemini", config);
    EXPECT_EQ(nullptr, selection.provider.get());
}

// ---------------------------------------------------------------------------
// Model defaults — every kind that uses a model gets the right default
// when `model` is absent from the config.
// ---------------------------------------------------------------------------

TEST(MakeProvider, AnthropicDefaultModelApplied) {
    Value config;
    config["key"] = "test-key";
    const auto selection = makeProvider("anthropic", config);
    EXPECT_EQ("claude-sonnet-4-6", selection.model);
}

TEST(MakeProvider, OllamaDefaultModelApplied) {
    const Value config;
    EXPECT_EQ("llama3.2", makeProvider("ollama", config).model);
}

TEST(MakeProvider, LmStudioDefaultModelApplied) {
    const Value config;
    EXPECT_EQ("local-model", makeProvider("lm-studio", config).model);
}

TEST(MakeProvider, ClaudeCliDefaultModelApplied) {
    const Value config;
    EXPECT_EQ("sonnet", makeProvider("claude-cli", config).model);
}

TEST(MakeProvider, GeminiDefaultModelApplied) {
    Value config;
    config["key"] = "test-key";
    EXPECT_EQ("gemini-2.5-flash", makeProvider("gemini", config).model);
}

// ---------------------------------------------------------------------------
// Explicit `model` override beats the default for every kind.
// ---------------------------------------------------------------------------

TEST(MakeProvider, ExplicitModelOverridesAnthropicDefault) {
    Value config;
    config["key"] = "test-key";
    config["model"] = "claude-opus-4-7";
    EXPECT_EQ("claude-opus-4-7", makeProvider("anthropic", config).model);
}

TEST(MakeProvider, ExplicitModelOverridesOllamaDefault) {
    Value config;
    config["model"] = "mistral";
    EXPECT_EQ("mistral", makeProvider("ollama", config).model);
}

// ---------------------------------------------------------------------------
// Unknown `kind` falls back to anthropic (matches AiManager's prior behaviour).
// ---------------------------------------------------------------------------

TEST(MakeProvider, UnknownKindWithKeyFallsBackToAnthropic) {
    Value config;
    config["key"] = "test-key";
    const auto selection = makeProvider("does-not-exist", config);
    EXPECT_TRUE(isInstanceOf<AnthropicProvider>(selection.provider.get()));
    EXPECT_EQ("claude-sonnet-4-6", selection.model);
}

TEST(MakeProvider, UnknownKindWithoutKeyReturnsNullProvider) {
    // Same fall-back path as anthropic — null provider when key missing.
    const Value config;
    const auto selection = makeProvider("totally-bogus", config);
    EXPECT_EQ(nullptr, selection.provider.get());
}

// ---------------------------------------------------------------------------
// Optional provider-specific config is accepted without crashing — the
// factory's job is to feed the value into the right ctor; the provider
// then trusts it.
// ---------------------------------------------------------------------------

TEST(MakeProvider, OllamaAcceptsCustomEndpoint) {
    Value config;
    config["endpoint"] = "http://192.168.1.100:11434";
    const auto selection = makeProvider("ollama", config);
    EXPECT_TRUE(isInstanceOf<OllamaProvider>(selection.provider.get()));
}

TEST(MakeProvider, LmStudioAcceptsCustomEndpointAndKey) {
    Value config;
    config["endpoint"] = "http://192.168.1.50:1234";
    config["key"] = "lm-studio-bearer";
    config["model"] = "qwen2.5-coder";
    const auto selection = makeProvider("lm-studio", config);
    EXPECT_TRUE(isInstanceOf<LmStudioProvider>(selection.provider.get()));
    EXPECT_EQ("qwen2.5-coder", selection.model);
}

TEST(MakeProvider, ClaudeCliAcceptsCustomPath) {
    Value config;
    config["claudePath"] = "/opt/homebrew/bin/claude";
    const auto selection = makeProvider("claude-cli", config);
    EXPECT_TRUE(isInstanceOf<ClaudeCliProvider>(selection.provider.get()));
}
