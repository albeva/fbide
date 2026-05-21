//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Per-category storage policy for a `ConfigManager` entry.
///
/// Two modes:
/// - **Overlay** — bundle baseline at `basePath()` plus a writable
///   `.local.ini` overlay at `overlayPath()`. Loads merge the two; saves
///   write only divergences from baseline into the overlay (aggressively
///   pruned). This is the default-boot mode.
/// - **Direct** — single file at `basePath()`, no overlay. Loads and
///   saves both target that file. Used when the user passed
///   `--config=PATH` or invoked `reloadConfig(path)` at runtime — the
///   user has taken explicit ownership of the file and reproducibility
///   (e.g. CI / repro runs) trumps upgrade-tracking.
///
/// Value semantics: copyable + movable so `ConfigManager::Entry` can
/// keep one without ceremony, and runtime mutations (`setCategoryPath`,
/// `reloadConfig`) can swap a strategy in place.
class [[nodiscard]] ConfigStrategy final {
public:
    /// Default-constructed strategy — `Direct` mode with empty paths.
    /// Used as a placeholder for category slots that haven't been
    /// loaded yet; `ConfigManager` overwrites it during `load()`.
    ConfigStrategy() = default;

    /// Bundle baseline + writable overlay. Both paths must be absolute
    /// and resolved by the caller — this type does no path resolution.
    [[nodiscard]] static auto overlay(wxString basePath, wxString overlayPath) -> ConfigStrategy {
        return { Mode::Overlay, std::move(basePath), std::move(overlayPath) };
    }

    /// Single-file mode — bundle baseline bypassed, saves overwrite the
    /// file in place.
    [[nodiscard]] static auto direct(wxString path) -> ConfigStrategy {
        return { Mode::Direct, std::move(path), {} };
    }

    /// Compute the `<base>.local.ini` overlay path for a given base file.
    /// Inserts `.local` before the extension so theme `.fbt` overlays
    /// would round-trip identically (themes don't actually use overlays
    /// today, but the rule generalises cleanly).
    /// - `readOnly` true  → overlay lives in `userDataDir`.
    /// - `readOnly` false → overlay lives next to `basePath`.
    [[nodiscard]] static auto deriveOverlayPath(
        const wxString& basePath,
        const wxString& userDataDir,
        bool readOnly
    ) -> wxString;

    /// Top-level strategy rule.
    /// - `overlayCapable=false` (e.g. locale)            → `direct(basePath)`.
    /// - `explicitMode=true` (`--config=PATH` or runtime
    ///   `reloadConfig`)                                  → `direct(basePath)`.
    /// - otherwise (default boot, mutable category)      → `overlay(basePath,
    ///   deriveOverlayPath(basePath, userDataDir, readOnly))`.
    [[nodiscard]] static auto select(
        const wxString& basePath,
        const wxString& userDataDir,
        bool readOnly,
        bool overlayCapable,
        bool explicitMode
    ) -> ConfigStrategy;

    /// Immutable baseline. Always populated regardless of mode.
    [[nodiscard]] auto basePath() const noexcept -> const wxString& { return m_basePath; }

    /// Writable overlay path; empty when the strategy is `Direct`.
    [[nodiscard]] auto overlayPath() const noexcept -> const wxString& { return m_overlayPath; }

    /// Path to write on `save()` — overlay in `Overlay` mode, base in
    /// `Direct` mode. Encodes the save-target rule so `ConfigManager`
    /// can stay mode-agnostic.
    [[nodiscard]] auto savePath() const noexcept -> const wxString& {
        return m_mode == Mode::Overlay ? m_overlayPath : m_basePath;
    }

    /// True when `Overlay` — caller must run merge on load and
    /// diff-against-baseline + prune on save.
    [[nodiscard]] auto usesOverlay() const noexcept -> bool { return m_mode == Mode::Overlay; }

private:
    enum class Mode : std::uint8_t { Overlay,
        Direct };

    ConfigStrategy(const Mode mode, wxString basePath, wxString overlayPath)
    : m_mode(mode)
    , m_basePath(std::move(basePath))
    , m_overlayPath(std::move(overlayPath)) {}

    Mode m_mode { Mode::Direct };
    wxString m_basePath {};
    wxString m_overlayPath {};
};

} // namespace fbide
