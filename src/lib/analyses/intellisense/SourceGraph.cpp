//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SourceGraph.hpp"
using namespace fbide;

auto SourceGraph::normalize(const std::filesystem::path& path) -> std::filesystem::path {
    return path.lexically_normal();
}

auto SourceGraph::hashSource(const std::string_view source) -> std::size_t {
    const auto hash = std::hash<std::string_view> {}(source);
    return hash == 0 ? 1 : hash; // reserve 0 for "no content yet"
}

auto SourceGraph::find(const std::filesystem::path& path) const -> SourceEntry* {
    const auto it = m_entries.find(normalize(path));
    return it != m_entries.end() ? it->second.get() : nullptr;
}

auto SourceGraph::getOrCreate(const std::filesystem::path& path) -> SourceEntry* {
    const auto key = normalize(path);
    if (const auto it = m_entries.find(key); it != m_entries.end()) {
        return it->second.get();
    }
    auto entry = std::make_unique<SourceEntry>();
    entry->path = key;
    auto* const raw = entry.get();
    m_entries.emplace(key, std::move(entry));
    return raw;
}

void SourceGraph::enqueue(SourceEntry* entry) {
    if (entry->queued || !entry->dirty()) {
        return;
    }
    entry->queued = true;
    m_queue.push_back(entry);
}

auto SourceGraph::openDocument(const std::filesystem::path& path, Document* document) -> SourceEntry* {
    auto* const entry = getOrCreate(path);
    entry->owner = document;
    return entry;
}

void SourceGraph::closeDocument(const std::filesystem::path& path, Document* document) {
    auto* const entry = find(path);
    if (entry == nullptr || entry->owner != document) {
        return; // unknown file, or owned by a different document — leave it be
    }
    entry->owner = nullptr;
    collectOrphans();
}

void SourceGraph::submit(const std::filesystem::path& path, std::string source) {
    auto* const entry = getOrCreate(path);
    const auto stamp = hashSource(source);
    if (stamp == entry->contentStamp) {
        return; // identical to current content — already set / queued / parsed
    }
    entry->source = std::move(source);
    entry->contentStamp = stamp;
    enqueue(entry);
}

auto SourceGraph::setIncludes(SourceEntry* entry, const std::vector<std::filesystem::path>& resolved)
    -> std::vector<SourceEntry*> {
    // Normalise the desired set so comparisons match the stored keys.
    std::vector<std::filesystem::path> desired;
    desired.reserve(resolved.size());
    for (const auto& path : resolved) {
        auto key = normalize(path);
        if (key != entry->path) { // ignore a file that includes itself
            desired.push_back(std::move(key));
        }
    }

    // Drop edges no longer present, fixing the child's parent back-link.
    std::erase_if(entry->includes, [&](SourceEntry* child) {
        if (std::ranges::find(desired, child->path) != desired.end()) {
            return false;
        }
        std::erase(child->parents, entry);
        return true;
    });

    // Wire added edges; re-enqueue already-known targets whose content is stale.
    std::vector<SourceEntry*> created;
    for (const auto& path : desired) {
        if (std::ranges::any_of(entry->includes, [&](SourceEntry* child) { return child->path == path; })) {
            enqueue(find(path)); // existing edge — parse if it has gone stale
            continue;
        }
        const bool existed = find(path) != nullptr;
        auto* const child = getOrCreate(path);
        entry->includes.push_back(child);
        child->parents.push_back(entry);
        if (existed) {
            enqueue(child);
        } else {
            created.push_back(child); // brand new — caller supplies content
        }
    }
    return created;
}

auto SourceGraph::takeNext() -> SourceEntry* {
    if (m_queue.empty()) {
        return nullptr;
    }
    auto* const entry = m_queue.front();
    m_queue.erase(m_queue.begin());
    entry->queued = false;
    return entry;
}

void SourceGraph::collectOrphans() {
    // Mark everything reachable from an owned document via include edges.
    std::unordered_set<const SourceEntry*> reachable;
    std::vector<SourceEntry*> stack;
    for (const auto& entry : m_entries | std::views::values) {
        if (entry->isOwned()) {
            stack.push_back(entry.get());
        }
    }
    while (!stack.empty()) {
        const auto* const entry = stack.back();
        stack.pop_back();
        if (!reachable.insert(entry).second) {
            continue; // already visited — cycle-safe
        }
        for (auto* const child : entry->includes) {
            stack.push_back(child);
        }
    }
    if (reachable.size() == m_entries.size()) {
        return; // nothing to sweep
    }

    // A surviving entry's `includes` are always reachable (a marked node's
    // children are marked), but its `parents` may include a now-swept file, so
    // scrub those back-links before deleting. Then drop swept entries from the
    // queue and the store.
    for (auto& entry : m_entries | std::views::values) {
        if (reachable.contains(entry.get())) {
            std::erase_if(entry->parents, [&](SourceEntry* parent) { return !reachable.contains(parent); });
        }
    }
    std::erase_if(m_queue, [&](SourceEntry* entry) { return !reachable.contains(entry); });
    std::erase_if(m_entries, [&](const auto& kv) { return !reachable.contains(kv.second.get()); });
}
