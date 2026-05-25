//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
} // namespace fbide

namespace fbide::ai {
class AiManager;

/**
 * Owns one `AiManager` per provider configured under `[ai.<name>]`.
 *
 * The chat notebook builds one tab per entry; the registry tracks which
 * tab is active and persists that choice back to `[ai] active` when the
 * user switches tabs. There is always at least one entry — when no
 * provider sections are configured, the registry synthesizes a placeholder
 * manager so `Context::getAiManager()` callers never see null.
 *
 * **Owns:** every `AiManager` for the running session.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 */
class AiManagerRegistry final {
public:
    NO_COPY_AND_MOVE(AiManagerRegistry)

    /// One entry in the registry — the manager plus the metadata the
    /// notebook needs to render its tab.
    struct Entry {
        std::unique_ptr<AiManager> manager; ///< The provider-bound manager (never null).
        wxString configName;                ///< `[ai.<name>]` section key (empty for the placeholder).
        wxString displayName;               ///< Tab label — `name` from config, falling back to `configName`.
    };

    /// Build entries from the current config. Reads every `[ai.<name>]`
    /// subsection (sorted by name for stable tab order), constructs an
    /// `AiManager` for each, and selects the entry whose `configName`
    /// matches `[ai] active`. Synthesizes a single placeholder entry when
    /// no provider sections exist.
    explicit AiManagerRegistry(Context& ctx);
    ~AiManagerRegistry();

    /// Number of entries — always `>= 1` (the placeholder counts).
    [[nodiscard]] auto count() const -> std::size_t { return m_entries.size(); }

    /// Read-only access to all entries — used by `UIManager` to build tabs.
    [[nodiscard]] auto entries() const -> const std::vector<Entry>& { return m_entries; }

    /// Index of the active entry. Always in range when `count() > 0`.
    [[nodiscard]] auto activeIndex() const -> std::size_t { return m_activeIndex; }

    /// The currently active manager — returned by `Context::getAiManager()`.
    [[nodiscard]] auto active() -> AiManager&;
    [[nodiscard]] auto active() const -> const AiManager&;

    /// Switch the active entry by index. When `persist` is true, the new
    /// config name is written to `[ai] active` and the Config category is
    /// flushed to disk so the choice survives a restart. No-op when
    /// `index` is out of range or already active.
    void setActiveIndex(std::size_t index, bool persist = true);

private:
    Context& m_ctx;                ///< Application context — used to read/write config.
    std::vector<Entry> m_entries;  ///< One entry per `[ai.<name>]` section (sorted by name).
    std::size_t m_activeIndex = 0; ///< Index into `m_entries` — defaults to the first when `[ai] active` is missing.
};

} // namespace fbide::ai
