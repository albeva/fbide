//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManager.hpp"
#include "AnthropicProvider.hpp"
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
} // namespace

AiManager::AiManager(Context& ctx)
: m_ctx(ctx) {
    // Provider config lives under `[ai]` in the preferences:
    //   provider = anthropic | ollama   (default: anthropic)
    //   model    = <model name>
    //   key      = <Anthropic API key>  (anthropic only — plaintext, see
    //              docs/ai-chat-plan.md; OS keychain is deferred)
    //   endpoint = <Ollama base URL>    (ollama only)
    const auto& config = m_ctx.getConfigManager().config();
    const auto provider = config.at("ai.provider").value_or("anthropic");

    if (provider == "ollama") {
        m_model = config.at("ai.model").value_or(kDefaultOllamaModel);
        const auto endpoint = config.at("ai.endpoint").value_or(kDefaultOllamaEndpoint);
        m_provider = std::make_unique<OllamaProvider>(endpoint);
    } else {
        m_model = config.at("ai.model").value_or(kDefaultAnthropicModel);
        if (const auto key = config.at("ai.key").as<wxString>(); key && !key->empty()) {
            m_provider = std::make_unique<AnthropicProvider>(*key);
        }
    }
}

AiManager::~AiManager() = default;

void AiManager::sendMessage(const wxString& text, AiProvider::ResponseHandler handler) {
    if (m_provider == nullptr) {
        handler(AiResponse {
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

    m_provider->send(request, [this, handler = std::move(handler)](AiResponse response) {
        if (response.ok) {
            m_history.push_back({ .role = AiRole::Assistant, .content = response.text });
        }
        handler(std::move(response));
    });
}
