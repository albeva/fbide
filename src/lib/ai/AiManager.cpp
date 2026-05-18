//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManager.hpp"
#include "AnthropicProvider.hpp"
#include "ClaudeCliProvider.hpp"
#include "GeminiProvider.hpp"
#include "OllamaProvider.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

namespace {
// Defaults applied when the matching `[ai]` key is absent. Declared as
// arrays (not `const char*`) so they bind to `Value::value_or`'s string-
// literal overload rather than the `bool` one.
constexpr char kDefaultAnthropicModel[] = "claude-sonnet-4-6";
constexpr char kDefaultOllamaModel[] = "llama3.2";
constexpr char kDefaultOllamaEndpoint[] = "http://localhost:11434";
constexpr char kDefaultClaudeModel[] = "sonnet";
constexpr char kDefaultClaudePath[] = "claude";
constexpr char kDefaultGeminiModel[] = "gemini-2.5-flash";
} // namespace

AiManager::AiManager(Context& ctx)
: m_ctx(ctx) {
    // AI config in the preferences uses a named-config layout:
    //
    //   [ai]
    //   active = <config-name>           selects which config below to use
    //
    //   [ai/<config-name>]               one section per named config
    //   provider   = anthropic | ollama | claude-cli | gemini
    //   model      = <model name>
    //   key        = <API key>           (anthropic + gemini — plaintext,
    //                see docs/ai-chat-plan.md; OS keychain is deferred)
    //   endpoint   = <Ollama base URL>   (ollama only)
    //   claudePath = <path to claude>    (claude-cli only)
    //
    // Only the `active` config is used. There is no hot-reload — the
    // provider is resolved once here, at construction.
    const auto& root = m_ctx.getConfigManager().config();

    const auto active = root.at("ai.active").as<wxString>();
    if (!active || active->empty()) {
        return; // No active config — `isReady()` stays false.
    }

    const auto& config = root.at("ai." + *active);
    const auto provider = config.at("provider").value_or("anthropic");

    if (provider == "ollama") {
        m_model = config.at("model").value_or(kDefaultOllamaModel);
        const auto endpoint = config.at("endpoint").value_or(kDefaultOllamaEndpoint);
        m_provider = std::make_unique<OllamaProvider>(endpoint);
    } else if (provider == "claude-cli") {
        m_model = config.at("model").value_or(kDefaultClaudeModel);
        const auto path = config.at("claudePath").value_or(kDefaultClaudePath);
        m_provider = std::make_unique<ClaudeCliProvider>(path);
    } else if (provider == "gemini") {
        m_model = config.at("model").value_or(kDefaultGeminiModel);
        if (const auto key = config.at("key").as<wxString>(); key && !key->empty()) {
            m_provider = std::make_unique<GeminiProvider>(*key);
        }
    } else {
        m_model = config.at("model").value_or(kDefaultAnthropicModel);
        if (const auto key = config.at("key").as<wxString>(); key && !key->empty()) {
            m_provider = std::make_unique<AnthropicProvider>(*key);
        }
    }
}

AiManager::~AiManager() = default;

void AiManager::sendMessage(const wxString& text, AiProvider::ChunkHandler onChunk, AiProvider::ResponseHandler onComplete) {
    if (m_provider == nullptr) {
        onComplete(AiResponse {
            .ok = false,
            .text = {},
            .error = "No AI provider configured. Check the [ai] section in the preferences.",
        });
        return;
    }

    m_history.push_back({ .role = AiRole::User, .content = text });

    AiRequest request;
    request.model = m_model;
    request.messages = m_history;

    // Accumulate the streamed deltas so the full reply can be stored in
    // the history once the request completes.
    auto accumulated = std::make_shared<wxString>();

    m_provider->send(
        request,
        [onChunk = std::move(onChunk), accumulated](const wxString& delta) {
            *accumulated += delta;
            onChunk(delta);
        },
        [this, accumulated, onComplete = std::move(onComplete)](AiResponse response) {
            if (response.ok) {
                // Prefer the streamed text; fall back to a non-streamed reply.
                const wxString& full = accumulated->empty() ? response.text : *accumulated;
                m_history.push_back({ .role = AiRole::Assistant, .content = full });
            }
            onComplete(std::move(response));
        }
    );
}
