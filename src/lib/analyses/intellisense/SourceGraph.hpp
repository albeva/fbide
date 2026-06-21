//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

class Document;
class SymbolTable;

/// One file in the source/include graph: an open document or a pure `#include`.
/// Keyed by a normalised path. `owner` is an opaque identity tag — the worker
/// never dereferences it. Edges run both ways: `includes` (files this one pulls
/// in) and `parents` (files that pull this one in).
struct SourceEntry {
    std::filesystem::path path;               ///< Normalised key.
    Document* owner = nullptr;                ///< Owning open document (opaque), or null for a pure include.
    std::string source;                       ///< Current UTF-8 source (buffer snapshot or disk read).
    std::size_t contentStamp = 0;             ///< Hash of `source`; 0 means no content yet.
    std::size_t parsedStamp = 0;              ///< `contentStamp` the `symbolTable` was built from.
    std::shared_ptr<SymbolTable> symbolTable; ///< Latest parse result, or null (worker-owned).
    std::vector<SourceEntry*> includes;       ///< Files this includes (out-edges).
    std::vector<SourceEntry*> parents;        ///< Files that include this (in-edges).
    bool queued = false;                      ///< Currently in the work queue.

    /// True when the source has changed since the symbol table was built.
    [[nodiscard]] auto dirty() const noexcept -> bool { return contentStamp != parsedStamp; }
    /// True when an open document owns this entry.
    [[nodiscard]] auto isOwned() const noexcept -> bool { return owner != nullptr; }
};

/// Owns the source/include graph for the intellisense worker: a path-keyed set
/// of `SourceEntry` nodes with include/parent edges, a de-duplicated work queue
/// (every file appears once — all includes are treated as `#include once`), and
/// cycle-safe orphan collection.
///
/// Structure only — no file I/O, no parsing, no threading; the worker is the
/// sole owner and drives it. The caller supplies content via `submit`, resolved
/// include paths via `setIncludes`, and drains work via `takeNext`.
class SourceGraph final {
public:
    /// Associate an open document with its file (creating the entry if absent)
    /// and mark it owned. Returns the entry. An existing pure-include entry is
    /// reused — its symbol table, edges and content are kept.
    auto openDocument(const std::filesystem::path& path, Document* document) -> SourceEntry*;

    /// Disassociate `document` from its file, then collect any entries no longer
    /// reachable from an open document (cycle-safe). No-op when the entry is not
    /// owned by `document`.
    void closeDocument(const std::filesystem::path& path, Document* document);

    /// Set an entry's source snapshot. Enqueues it for parsing only when the
    /// content actually changed, so unchanged re-submits are ignored.
    void submit(const std::filesystem::path& path, std::string source);

    /// Replace `entry`'s include edges with `resolved` (diffing): wire added
    /// targets (creating empty entries as needed) and unwire dropped ones,
    /// keeping `parents` consistent. Enqueues already-known targets whose content
    /// is stale. Returns entries newly created here — the caller must supply
    /// their content via `submit`. Targets are de-duplicated to one entry each.
    auto setIncludes(SourceEntry* entry, const std::vector<std::filesystem::path>& resolved)
        -> std::vector<SourceEntry*>;

    /// Entry for `path`, or null.
    [[nodiscard]] auto find(const std::filesystem::path& path) const -> SourceEntry*;

    /// Pop the next queued (dirty) entry, or null when the queue is empty.
    auto takeNext() -> SourceEntry*;

    /// True when the work queue is empty.
    [[nodiscard]] auto idle() const noexcept -> bool { return m_queue.empty(); }

    /// Entry count (open documents + includes). For tests / diagnostics.
    [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

    /// Remove every entry unreachable (via include edges) from an open document.
    /// Cycle-safe: an unowned cycle with no owned ancestor is removed.
    void collectOrphans();

    /// Paths of every pure `#include` entry (owner == nullptr) — the closed
    /// include files currently in the graph. Returns a normalised value copy;
    /// worker-thread only. The UI uses it to reconcile its filesystem watches.
    [[nodiscard]] auto pureIncludePaths() const -> std::vector<std::filesystem::path>;

    /// Force every entry that has content to re-parse (re-resolving its includes)
    /// — used when the include search dirs change, since resolution depends on
    /// them. Content-less entries (awaiting a disk read) are left untouched.
    void reparseAll();

private:
    auto getOrCreate(const std::filesystem::path& path) -> SourceEntry*;
    /// Like `getOrCreate` but takes an already-normalised key (no re-normalise,
    /// single map probe). Returns the entry and whether it was just created.
    auto getOrCreateNormalized(std::filesystem::path key) -> std::pair<SourceEntry*, bool>;
    void enqueue(SourceEntry* entry);
    [[nodiscard]] static auto normalize(const std::filesystem::path& path) -> std::filesystem::path;
    [[nodiscard]] static auto hashSource(std::string_view source) -> std::size_t;

    std::unordered_map<std::filesystem::path, std::unique_ptr<SourceEntry>> m_entries;
    std::vector<SourceEntry*> m_queue; ///< Dirty entries awaiting parse; deduped via SourceEntry::queued.
    /// Set when an include edge is dropped or a document closed — the only events
    /// that can orphan a subgraph. `collectOrphans` skips its whole-graph sweep
    /// while this is clear, so a pure content edit never walks the graph.
    bool m_maybeOrphans = false;
};

} // namespace fbide
