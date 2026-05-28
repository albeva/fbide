//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class ConfigManager;

/// Canonical (always-present) configuration's slug.
inline constexpr auto kCanonicalCompilerSlug = "default";

/// A compiler configuration with every field resolved through the base
/// chain. The four `*Command` / path fields are what `CompileCommand` /
/// `RunCommand` consume directly (meta-tags `<$fbc>` / `<$file>` etc.
/// remain unexpanded — expansion is the caller's job).
struct ResolvedCompilerConfig {
    wxString slug;                 ///< `"default"` for canonical, else opaque user slug.
    wxString displayName;          ///< User-facing label shown in the toolbar combobox.
    std::filesystem::path path;    ///< `fbc` binary (resolved through base chain).
    wxString runCommand;           ///< Template for executing the built binary.
    wxString compileCommand;       ///< Template for invoking the compiler.
    wxString terminal;             ///< Terminal-launcher prefix for run targets.
};

/// Schema-level identity of the four overridable fields. Used by Phase 6
/// CRUD; declared here so consumers don't need a second header.
enum class CompilerField : std::uint8_t {
    Path,
    CompileCommand,
    RunCommand,
    Terminal,
};

/// Read-only view over the compiler configurations declared in
/// `config_<plat>.ini` — `[compiler]` is the canonical Default, every
/// `[compiler/<slug>]` is a user-defined entry that inherits unspecified
/// fields from its `base=` and ultimately from canonical.
///
/// **Owns:** the resolved configs cache, rebuilt on `reload()`.
/// **Owned by:** `CompilerManager`.
/// **Threading:** UI thread only.
///
/// Inheritance is unbounded in depth. Cycles or orphan bases are tolerated
/// at load time — the offending configs stay in the catalog but resolve
/// their unspecified fields against canonical, with a `wxLogWarning`
/// describing the slug and reason. This way the settings UI can show the
/// broken entry and let the user fix it instead of failing the boot.
class CompilerConfigCatalog final {
public:
    NO_COPY_AND_MOVE(CompilerConfigCatalog)

    /// Construct from a live `ConfigManager`. The ctor does not read
    /// config — call `reload()` once after construction (and after any
    /// CRUD mutation in later phases).
    explicit CompilerConfigCatalog(ConfigManager& cfg);
    ~CompilerConfigCatalog() = default;

    /// Re-parse `[compiler]` + every `[compiler/*]` section and rebuild
    /// the resolved cache.
    void reload();

    /// Canonical Default — always present, always at `all()[0]`.
    [[nodiscard]] auto canonical() const -> const ResolvedCompilerConfig&;

    /// Look up by slug (canonical or user). Returns `nullptr` when the
    /// slug is unknown.
    [[nodiscard]] auto find(const wxString& slug) const -> const ResolvedCompilerConfig*;

    /// Canonical first, then user-defined configs sorted by the numeric
    /// suffix of their `cfg-N` slug.
    [[nodiscard]] auto all() const -> std::span<const ResolvedCompilerConfig>;

    /// Slug of the active configuration — `compiler.active` from the INI,
    /// or `"default"` when the key is unset or refers to a missing slug.
    /// A missing-slug fallback emits a `wxLogWarning`.
    [[nodiscard]] auto activeSlug() const -> wxString;

    /// Resolve which configuration to use for a document whose pinned
    /// slug is `pinnedSlug` (matches `Document::getConfiguration()`):
    ///   - has value, slug exists → that configuration
    ///   - has value, slug missing → active configuration (warning)
    ///   - empty → active configuration
    /// If even the active slug fails to resolve, falls back to canonical.
    [[nodiscard]] auto resolveByPinnedSlug(const std::optional<wxString>& pinnedSlug) const
        -> const ResolvedCompilerConfig&;

    /// Normalisation for the "picked from toolbar" event: returns
    /// `nullopt` when `pickedSlug` matches the active slug (so the
    /// document follows the active), otherwise the slug verbatim
    /// (pinned).
    [[nodiscard]] auto normalizeForStorage(const wxString& pickedSlug) const
        -> std::optional<wxString>;

    // ---------------------------------------------------------------
    // CRUD — every mutation reloads the in-memory cache so the next
    // read sees the new state. Callers are responsible for persisting
    // via `ConfigManager::save(Category::Config)` once their edit
    // session is complete.
    // ---------------------------------------------------------------

    /// Allocate a fresh `cfg-N` slug and create an empty user-defined
    /// configuration with the given display name. Returns the slug.
    auto createFromCanonical(const wxString& displayName) -> wxString;

    /// Deep-copy the overrides + base of `sourceSlug` into a fresh
    /// `cfg-N` slot with the given display name. Returns the new slug.
    /// `sourceSlug` may be the canonical default — the copy will then
    /// have no overrides and `base=` empty (inherits from canonical).
    auto copy(const wxString& sourceSlug, const wxString& displayName) -> wxString;

    /// Remove a user-defined configuration. Any other config whose
    /// `base=` was the removed slug is re-parented to canonical
    /// (`base=` key cleared). If `compiler.active` was the removed
    /// slug, it is cleared too. Removing canonical is rejected — pass
    /// any other slug. Returns true on success.
    auto remove(const wxString& slug) -> bool;

    /// Update a configuration's display name (`name=` key). No-op for
    /// canonical default. Returns true if the config existed.
    auto rename(const wxString& slug, const wxString& displayName) -> bool;

    /// Change a configuration's base. Rejects self-base and any base
    /// that is a transitive descendant of `slug` (would create a
    /// cycle). Returns true on success.
    auto setBase(const wxString& slug, const wxString& newBaseSlug) -> bool;

    /// Set or clear a single field override. `nullopt` removes the
    /// key entirely (the field will inherit from the base); a non-
    /// empty optional writes the value (an empty string still
    /// counts as an explicit override).
    auto setOverride(const wxString& slug, CompilerField field, const std::optional<wxString>& value) -> bool;

    /// Set `compiler.active`. Passing `"default"` clears the key.
    void setActiveSlug(const wxString& slug);

    /// Slugs that may legally be the base of `slug` (excludes the
    /// slug itself and all of its transitive descendants). Canonical
    /// `"default"` is always included.
    [[nodiscard]] auto validBasesFor(const wxString& slug) const -> std::vector<wxString>;

private:
    ConfigManager& m_cfg;
    /// Canonical at index 0, user configs follow.
    std::vector<ResolvedCompilerConfig> m_configs;
};

} // namespace fbide
