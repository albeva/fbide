//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiManagerRegistry.hpp"
#include "AiManager.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Value.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Collect the names of every `[ai.<name>]` subsection that looks like a
/// provider definition (has a `provider` key). Sorted alphabetically so
/// tab order is stable across launches — `Value::Table` is an unordered
/// map, so iteration order alone is not.
auto collectProviderNames(const Value& aiSection) -> std::vector<wxString> {
    std::vector<wxString> names;
    if (!aiSection.isTable()) {
        return names;
    }
    for (const auto& [key, child] : aiSection.entries()) {
        if (child && child->isTable() && child->at("provider").as<wxString>()) {
            names.push_back(key);
        }
    }
    std::ranges::sort(names);
    return names;
}

/// Derive the tab label for `[ai.<configName>]`: explicit `name` field
/// when present, falling back to the section name itself.
auto resolveDisplayName(const Value& aiSection, const wxString& configName) -> wxString {
    if (const auto explicitName = aiSection.at(configName).at("name").as<wxString>();
        explicitName && !explicitName->empty()) {
        return *explicitName;
    }
    return configName;
}

} // namespace

AiManagerRegistry::AiManagerRegistry(Context& ctx)
: m_ctx(ctx) {
    const auto& aiSection = m_ctx.getConfigManager().config().at("ai");
    auto names = collectProviderNames(aiSection);

    if (names.empty()) {
        // No providers configured — synthesize a single placeholder so
        // `active()` is always callable. The placeholder's `isReady()`
        // stays false; the chat panel surfaces the "no provider
        // configured" hint when the user tries to send.
        m_entries.push_back({
            .manager = std::make_unique<AiManager>(m_ctx, wxString {}),
            .configName = {},
            .displayName = m_ctx.tr("panels.aichat.title"),
        });
        return;
    }

    m_entries.reserve(names.size());
    for (auto& name : names) {
        auto displayName = resolveDisplayName(aiSection, name);
        m_entries.push_back({
            .manager = std::make_unique<AiManager>(m_ctx, name),
            .configName = std::move(name),
            .displayName = std::move(displayName),
        });
    }

    // Select the active entry. `[ai] active` is the user-set default; an
    // unknown / missing value falls back to the first entry so the UI
    // still has something to show.
    const auto active = aiSection.at("active").as<wxString>().value_or(wxString {});
    if (!active.empty()) {
        for (std::size_t i = 0; i < m_entries.size(); i++) {
            if (m_entries[i].configName == active) {
                m_activeIndex = i;
                break;
            }
        }
    }
}

AiManagerRegistry::~AiManagerRegistry() = default;

auto AiManagerRegistry::active() -> AiManager& {
    return *m_entries.at(m_activeIndex).manager;
}

auto AiManagerRegistry::active() const -> const AiManager& {
    return *m_entries.at(m_activeIndex).manager;
}

void AiManagerRegistry::setActiveIndex(const std::size_t index, const bool persist) {
    if (index >= m_entries.size() || index == m_activeIndex) {
        return;
    }
    m_activeIndex = index;
    if (!persist) {
        return;
    }
    // Persist the user's choice so the next launch opens on this tab.
    // The placeholder entry has an empty `configName`; writing it back
    // is a no-op as far as future runs are concerned (the registry
    // would synthesize a placeholder again anyway).
    auto& cfg = m_ctx.getConfigManager().config();
    cfg["ai"]["active"] = m_entries[m_activeIndex].configName;
    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
}
