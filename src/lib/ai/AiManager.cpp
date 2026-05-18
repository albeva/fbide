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
#include "MockProvider.hpp"
#include "OllamaProvider.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "format/renderers/HtmlRenderer.hpp"
#include "format/transformers/case/CaseTransform.hpp"
#include "format/transformers/reformat/ReFormatter.hpp"
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

    // One FreeBASIC lexer, configured once and reused for every chat code
    // block — see highlightFreeBasic.
    m_fbLexer = FBSciLexer::Create();
    lexer::configureFbWordlists(*m_fbLexer, m_ctx.getConfigManager().keywords().at("groups"));
}

AiManager::~AiManager() {
    if (m_fbLexer != nullptr) {
        m_fbLexer->Release();
    }
}

auto AiManager::highlightFreeBasic(const wxString& code, const bool reformat) -> wxString {
    const auto utf8 = code.utf8_string();

    // Lex over a headless MemoryDocument — same colouring path as the
    // editor. Only the document is per-call; the lexer is reused.
    MemoryDocument doc;
    doc.Set(std::string_view { utf8.data(), utf8.size() });
    m_fbLexer->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);

    lexer::MemoryDocStyledSource source(doc);
    lexer::StyleLexer adapter(source);
    auto tokens = adapter.tokenise();

    // Apply keyword case, then re-indent + re-format model code to the
    // editor's settings — the same pipeline as the Format dialog.
    if (reformat) {
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        const auto& caseConfig = m_ctx.getConfigManager().keywords().at("cases");
        for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
            const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
            cases[idx] = CaseMode::parse(caseConfig.get_or(key, "None").ToStdString()).value_or(CaseMode::None);
        }
        CaseTransform caseTransform(cases);
        tokens = caseTransform.apply(tokens);

        reformat::ReFormatter formatter(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(m_ctx.getConfigManager().config().get_or("editor.tabSize", 4)),
            .reIndent = true,
            .reFormat = true,
        });
        tokens = formatter.apply(tokens);
    }

    // Table wrapper — wxHtmlWindow only paints the code background that way.
    return HtmlRenderer(m_ctx.getTheme(), utf8.size(), HtmlRenderer::Wrap::Table).render(tokens);
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

    // System prompt = the configured prompt (if any) followed by the
    // attached files, re-read fresh on every send so the model always
    // sees current file content. Empty when neither is set.
    request.system = m_systemPrompt;
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
