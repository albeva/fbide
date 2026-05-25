//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManager.hpp"
#include "Patch.hpp"
#include "ProviderFactory.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "tools/ReadFileTool.hpp"
using namespace fbide;
using namespace fbide::ai;

AiManager::AiManager(Context& ctx)
: m_ctx(ctx) {
    // read_file is always registered — it is read-only and bounded.
    // apply_patch and compile land in Phase 3 and Phase 5 respectively,
    // each gated by a runtime toggle (agent mode / allow-compile).
    m_tools.add(std::make_unique<ReadFileTool>(ctx.getDocumentManager()));

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
    // `[ai] enable_tools` — set to false to keep the model out of the
    // tool path entirely (useful when debugging the chat surface or
    // when running against a model that mis-handles tool_use blocks).
    m_enableTools = root.at("ai.enable_tools").as<bool>().value_or(true);

    const auto& config = root.at("ai." + *active);
    if (const auto overridePrompt = config.at("systemPrompt").as<wxString>()) {
        m_systemPrompt = *overridePrompt;
    }

    auto selection = makeProvider(config.at("provider").value_or("anthropic"), config);
    m_provider = std::move(selection.provider);
    m_model = std::move(selection.model);

    // Loop is created only when a provider is configured — it borrows
    // a reference, and we don't want to hold one to a null provider.
    if (m_provider != nullptr) {
        m_loop = std::make_unique<ToolDispatchLoop>(
            *m_provider,
            [this](AiToolCall call, Tool::ResultHandler handler) {
                m_tools.invoke(std::move(call), std::move(handler));
            }
        );
    }
}

auto AiManager::toolsEnabled() const -> bool {
    return m_enableTools && m_provider != nullptr && m_provider->supportsTools();
}

auto AiManager::isToolExposed(const wxString& toolName) -> bool {
    // Per-tool gating. read_file is always available; apply_patch and
    // compile gate on toggles that land in later phases. Anything
    // else (future tools) is denied by default — fail-safe.
    return toolName == "read_file";
}

auto AiManager::buildRequest() const -> AiRequest {
    AiRequest request;
    request.model = m_model;
    request.messages = m_history;

    // System prompt = the configured prompt (if any), the agent-mode
    // instructions (when on), and the attached context items. Built
    // fresh on every dispatch turn so the model always sees current
    // file content and current mode. Empty when none of the three
    // are set.
    //
    // The base prompt and agent rubric are marked cacheable — they
    // change rarely between turns. Attached items carry their own
    // cacheable flag (true for on-disk files, false for buffer
    // snapshots). Providers that support prompt caching attach a
    // cache_control breakpoint per cacheable block; others fold the
    // vector back to a string via `joinSystem`.
    if (!m_systemPrompt.empty()) {
        request.system.push_back({ .text = m_systemPrompt, .cacheable = true });
    }
    if (m_agentMode && m_context.editTarget() != nullptr) {
        request.system.push_back({
            .text = "Agent mode is on. When the user asks for changes to the edit "
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
                    "diff.",
            .cacheable = true,
        });
    }
    for (auto& block : m_context.buildBlocks()) {
        request.system.push_back(std::move(block));
    }

    if (toolsEnabled()) {
        for (auto& descriptor : m_tools.descriptors()) {
            if (isToolExposed(descriptor.name)) {
                request.tools.push_back(std::move(descriptor));
            }
        }
    }
    return request;
}

void AiManager::sendMessage(const wxString& text, AiProvider::ChunkHandler onChunk, AiProvider::ResponseHandler onComplete) {
    if (m_provider == nullptr || m_loop == nullptr) {
        onComplete(AiResponse {
            .ok = false,
            .text = {},
            .error = "No AI provider configured. Check the [ai] section in the preferences.",
        });
        return;
    }

    m_history.push_back({ .role = AiRole::User, .content = text });

    m_loop->run(
        &m_history,
        [this] { return buildRequest(); },
        std::move(onChunk),
        std::move(onComplete)
    );
}

auto AiManager::patchKey(const wxString& search, const wxString& replace) -> std::size_t {
    // Hash each piece's UTF-8 bytes separately and fold via the shared
    // `hashCombine` helper. The previous form built a wxString concat
    // AND a UTF-8 string of it just to produce one hash — for multi-KB
    // patches that's two big allocations on every isPatchApplied check.
    const auto searchHash = std::hash<std::string> {}(search.utf8_string());
    const auto replaceHash = std::hash<std::string> {}(replace.utf8_string());
    return hashCombine(searchHash, replaceHash);
}

auto AiManager::applyPatch(const wxString& search, const wxString& replace, const bool recordAlways) -> bool {
    auto* document = m_ctx.getDocumentManager().getActive();
    if (document == nullptr) {
        if (recordAlways) {
            m_appliedPatches.insert(patchKey(search, replace));
        }
        return false;
    }

    auto* editor = document->getEditor();
    const auto sourceUtf8 = editor->GetText().utf8_string();
    const auto match = findPatchMatch(sourceUtf8, search, replace);
    if (match.offset < 0) {
        if (recordAlways) {
            m_appliedPatches.insert(patchKey(search, replace));
        }
        return false;
    }

    editor->BeginUndoAction();
    editor->SetTargetStart(match.offset);
    editor->SetTargetEnd(match.offset + match.length);
    editor->ReplaceTarget(match.replacement);
    editor->EndUndoAction();
    m_appliedPatches.insert(patchKey(search, replace));
    return true;
}

auto AiManager::isPatchApplied(const wxString& search, const wxString& replace) const -> bool {
    return m_appliedPatches.contains(patchKey(search, replace));
}
