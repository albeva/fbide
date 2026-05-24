//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProviderFactory.hpp"
#include "config/Value.hpp"
#include "provider/AnthropicProvider.hpp"
#include "provider/ClaudeCliProvider.hpp"
#include "provider/GeminiProvider.hpp"
#include "provider/MockProvider.hpp"
#include "provider/OllamaProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {
// Defaults applied when the matching `[ai/<name>]` key is absent. Declared
// as arrays (not `const char*`) so they bind to `Value::value_or`'s
// string-literal overload rather than the `bool` one.
constexpr char kDefaultAnthropicModel[] = "claude-sonnet-4-6";
constexpr char kDefaultOllamaModel[] = "llama3.2";
constexpr char kDefaultOllamaEndpoint[] = "http://localhost:11434";
constexpr char kDefaultClaudeModel[] = "sonnet";
constexpr char kDefaultClaudePath[] = "claude";
constexpr char kDefaultGeminiModel[] = "gemini-2.5-flash";
} // namespace

auto fbide::ai::makeProvider(const wxString& kind, const Value& config) -> ProviderSelection {
    if (kind == "ollama") {
        const auto endpoint = config.at("endpoint").value_or(kDefaultOllamaEndpoint);
        return {
            .provider = std::make_unique<OllamaProvider>(endpoint),
            .model = config.at("model").value_or(kDefaultOllamaModel),
        };
    }
    if (kind == "claude-cli") {
        const auto path = config.at("claudePath").value_or(kDefaultClaudePath);
        return {
            .provider = std::make_unique<ClaudeCliProvider>(path),
            .model = config.at("model").value_or(kDefaultClaudeModel),
        };
    }
    if (kind == "gemini") {
        ProviderSelection selection { .provider = nullptr, .model = config.at("model").value_or(kDefaultGeminiModel) };
        if (const auto key = config.at("key").as<wxString>(); key && !key->empty()) {
            selection.provider = std::make_unique<GeminiProvider>(*key);
        }
        return selection;
    }
    if (kind == "mock") {
        // Offline test provider — no model, no key.
        return { .provider = std::make_unique<MockProvider>(), .model = {} };
    }
    // Anthropic is the default for an unrecognised kind, matching the
    // previous behaviour of `AiManager`'s constructor.
    ProviderSelection selection { .provider = nullptr, .model = config.at("model").value_or(kDefaultAnthropicModel) };
    if (const auto key = config.at("key").as<wxString>(); key && !key->empty()) {
        selection.provider = std::make_unique<AnthropicProvider>(*key);
    }
    return selection;
}
