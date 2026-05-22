//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManager.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "provider/AnthropicProvider.hpp"
#include "provider/ClaudeCliProvider.hpp"
#include "provider/GeminiProvider.hpp"
#include "provider/MockProvider.hpp"
#include "provider/OllamaProvider.hpp"
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
    //   active       = <config-name>     selects which config below to use
    //   systemPrompt = <system prompt>   optional default system prompt
    //
    //   [ai/<config-name>]               one section per named config
    //   provider     = anthropic | ollama | claude-cli | gemini | mock
    //   model        = <model name>
    //   key          = <API key>         (anthropic + gemini — plaintext,
    //                  see docs/ai-chat-plan.md; OS keychain is deferred)
    //   endpoint     = <Ollama base URL> (ollama only)
    //   claudePath   = <path to claude>  (claude-cli only)
    //   systemPrompt = <system prompt>   optional — overrides [ai] systemPrompt
    //
    // Only the `active` config is used. There is no hot-reload — the
    // provider is resolved once here, at construction.
    const auto& root = m_ctx.getConfigManager().config();

    const auto active = root.at("ai.active").as<wxString>();
    if (!active || active->empty()) {
        return; // No active config — `isReady()` stays false.
    }

    // `[ai] systemPrompt` is the default; an `[ai/<name>] systemPrompt`
    // overrides it for that config. Nothing is baked in when both absent.
    m_systemPrompt = root.at("ai.systemPrompt").as<wxString>().value_or(wxString {});

    const auto& config = root.at("ai." + *active);
    if (const auto overridePrompt = config.at("systemPrompt").as<wxString>()) {
        m_systemPrompt = *overridePrompt;
    }
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
    } else if (provider == "mock") {
        // Offline test provider — no model, no key.
        m_provider = std::make_unique<MockProvider>();
    } else {
        m_model = config.at("model").value_or(kDefaultAnthropicModel);
        if (const auto key = config.at("key").as<wxString>(); key && !key->empty()) {
            m_provider = std::make_unique<AnthropicProvider>(*key);
        }
    }
}

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

    // System prompt = the configured prompt (if any), the agent-mode
    // instructions (when on), and the attached files. Built fresh on
    // every send so the model always sees current file content and
    // current mode. Empty when none of the three are set.
    request.system = m_systemPrompt;
    if (m_agentMode && m_context.editTarget() != nullptr) {
        if (!request.system.empty()) {
            request.system += "\n\n";
        }
        request.system += "Agent mode is on. When the user asks for changes to the edit "
                          "target file, reply with one or more SEARCH/REPLACE blocks "
                          "instead of describing the change. Each block looks like this, "
                          "with markers on their own lines:\n\n"
                          "<<<<<<< SEARCH\n"
                          "<exact text from the edit target to find>\n"
                          "=======\n"
                          "<text to replace it with>\n"
                          ">>>>>>> REPLACE\n\n"
                          "Rules:\n"
                          "- The SEARCH text must match the edit target byte-for-byte, "
                          "including indentation and trailing whitespace.\n"
                          "- Keep each block as small as is needed for an unambiguous "
                          "match; do not include unchanged context above or below.\n"
                          "- Multiple independent edits in the same reply each get their "
                          "own block. Emit them in source order.\n"
                          "- Prose around the blocks is fine, but the edits themselves "
                          "must appear in this exact format — not as fenced code or a "
                          "diff.";
    }
    if (!m_context.empty()) {
        if (!request.system.empty()) {
            request.system += "\n\n";
        }
        request.system += "The user has attached the following files as context:\n" + m_context.buildText();
    }

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
