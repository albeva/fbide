//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManager.hpp"
#include "AnthropicProvider.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

namespace {
// Default model when `[ai] model` is not set in the preferences. Declared
// as an array (not `const char*`) so it binds to `Value::value_or`'s
// string-literal overload rather than the `bool` one.
constexpr char kDefaultModel[] = "claude-sonnet-4-6";
} // namespace

AiManager::AiManager(Context& ctx)
: m_ctx(ctx) {
    // Provider config lives under `[ai]` in the preferences. The API key is
    // stored as plaintext for now — see docs/ai-chat-plan.md (deferred:
    // OS keychain). No key means no provider, and `isReady()` stays false.
    const auto& config = m_ctx.getConfigManager().config();
    m_model = config.at("ai.model").value_or(kDefaultModel);

    if (const auto key = config.at("ai.key").as<wxString>(); key && !key->empty()) {
        m_provider = std::make_unique<AnthropicProvider>(*key);
    }
}

AiManager::~AiManager() = default;

void AiManager::sendMessage(const wxString& text, AiProvider::ResponseHandler handler) {
    if (m_provider == nullptr) {
        handler(AiResponse {
            .ok = false,
            .text = {},
            .error = "No AI provider configured. Add an [ai] key entry to the preferences.",
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
