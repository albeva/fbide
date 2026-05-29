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

/// A compiler configuration with every field resolved against canonical
/// Default. The four `*Command` / path fields are what `CompileCommand` /
/// `RunCommand` consume directly (meta-tags `<$fbc>` / `<$file>` etc.
/// remain unexpanded — expansion is the caller's job).
struct ResolvedCompilerConfig {
    wxString slug;              ///< `"default"` for canonical, else opaque user slug.
    wxString displayName;       ///< User-facing label shown in the toolbar combobox.
    std::filesystem::path path; ///< Resolved `fbc` binary; user configs inherit canonical's when unset.
    wxString runCommand;        ///< Template for executing the built binary.
    wxString compileCommand;    ///< Template for invoking the compiler.
    wxString terminal;          ///< Terminal-launcher prefix for run targets.
    bool showInMenu = true;     ///< When false the config is hidden from the toolbar combobox and status-bar menu.
};

/// Schema-level identity of the four overridable fields.
enum class CompilerField : std::uint8_t {
    Path,
    CompileCommand,
    RunCommand,
    Terminal,
};

/// Every value of `CompilerField`, in declaration order. Use it to
/// iterate the overridable fields without re-listing them at each call
/// site.
inline constexpr std::array kAllCompilerFields {
    CompilerField::Path,
    CompilerField::CompileCommand,
    CompilerField::RunCommand,
    CompilerField::Terminal,
};

/// INI key under `[compiler]` / `[compiler/<slug>]` for a given field.
/// Inlined so callers in CompilerPage and the catalog impl share one
/// definition.
[[nodiscard]] inline auto compilerFieldKey(CompilerField field) -> wxString {
    switch (field) {
    case CompilerField::Path:
        return "path";
    case CompilerField::CompileCommand:
        return "compileCommand";
    case CompilerField::RunCommand:
        return "runCommand";
    case CompilerField::Terminal:
        return "terminal";
    }
    return wxString {};
}

/// Read-only view over the compiler configurations declared in
/// `config_<plat>.ini` — `[compiler]` is the canonical Default, every
/// `[compiler/<slug>]` is a user-defined entry whose unspecified fields
/// fall through to canonical.
///
/// **Owns:** the resolved configs cache, rebuilt on `reload()`.
/// **Owned by:** `CompilerManager`.
/// **Threading:** UI thread only.
///
/// Inheritance is one level deep: user configs inherit from canonical
/// Default only. Any `base=` key left over from older revisions of this
/// feature is silently ignored.
class CompilerConfigCatalog final {
public:
    NO_COPY_AND_MOVE(CompilerConfigCatalog)

    /// Construct from a live `ConfigManager`. The ctor does not read
    /// config — call `reload()` once after construction (and after any
    /// CRUD mutation).
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

    /// Subset of `all()` that should appear in the toolbar combobox and
    /// status-bar selector. Hidden configs (`showInMenu=false`) are
    /// filtered out, with one exception: `alwaysInclude` — typically the
    /// active document's pinned slug — is kept even when hidden so the
    /// selector can still display the current selection. Hidden does
    /// not mean unusable: a hidden config remains fully functional for
    /// any document already pinned to it.
    [[nodiscard]] auto menuConfigs(const wxString& alwaysInclude = {}) const -> std::vector<const ResolvedCompilerConfig*>;

    /// Config at position `index` within `all()`, or `nullptr` when the
    /// index is out of range. Lets list widgets that mirror `all()` map
    /// a selection index straight back to a config — no parallel slug
    /// array of their own.
    [[nodiscard]] auto at(int index) const -> const ResolvedCompilerConfig*;

    /// Position of `slug` within `all()`, or `-1` (matching
    /// `wxNOT_FOUND`) when absent. Inverse of `at()`; feeds straight
    /// into `wxControlWithItems::SetSelection`.
    [[nodiscard]] auto indexOf(const wxString& slug) const -> int;

    /// Slug of the active configuration — `compiler.active` from the INI,
    /// or `"default"` when the key is unset or refers to a missing slug.
    /// Resolved during `reload()`; a missing-slug fallback emits one
    /// `wxLogWarning` from `reload()`, not from every lookup.
    [[nodiscard]] auto activeSlug() const -> wxString;

    /// Resolve which configuration to use for a document whose pinned
    /// slug is `pinnedSlug` (matches `Document::getConfiguration()`):
    ///   - has value, slug exists → that configuration
    ///   - has value, slug missing → active configuration (warning)
    ///   - empty → active configuration
    /// If even the active slug fails to resolve, falls back to canonical.
    [[nodiscard]] auto resolveByPinnedSlug(const std::optional<wxString>& pinnedSlug) const -> const ResolvedCompilerConfig&;

    /// Normalisation for the "picked from toolbar" event: returns
    /// `nullopt` when `pickedSlug` matches the active slug (so the
    /// document follows the active), otherwise the slug verbatim
    /// (pinned).
    [[nodiscard]] auto normalizeForStorage(const wxString& pickedSlug) const -> std::optional<wxString>;

    // ---------------------------------------------------------------
    // CRUD — every mutation reloads the in-memory cache so the next
    // read sees the new state. Callers are responsible for persisting
    // via `ConfigManager::save(Category::Config)` once their edit
    // session is complete.
    // ---------------------------------------------------------------

    /// Allocate a fresh `cfg-N` slug and create an empty user-defined
    /// configuration with the given display name. Returns the slug.
    auto createFromCanonical(const wxString& displayName) -> wxString;

    /// Deep-copy the overrides of `sourceSlug` into a fresh `cfg-N`
    /// slot with the given display name. Returns the new slug.
    /// `sourceSlug` may be the canonical default — the copy will then
    /// have no overrides.
    auto copy(const wxString& sourceSlug, const wxString& displayName) -> wxString;

    /// Remove a user-defined configuration. If `compiler.active` was
    /// the removed slug, it is cleared. Removing canonical is rejected.
    /// Returns true on success.
    auto remove(const wxString& slug) -> bool;

    /// Update a configuration's display name (`name=` key). No-op for
    /// canonical default. Returns true if the config existed.
    auto rename(const wxString& slug, const wxString& displayName) -> bool;

    /// Set or clear a single field override. `nullopt` removes the
    /// key entirely (the field will inherit from canonical); a non-
    /// empty optional writes the value (an empty string still
    /// counts as an explicit override).
    auto setOverride(const wxString& slug, CompilerField field, const std::optional<wxString>& value) -> bool;

    /// Set `compiler.active`. Passing `"default"` clears the key.
    void setActiveSlug(const wxString& slug);

    /// Toggle whether a configuration appears in the toolbar combobox
    /// and status-bar selection menu. Persists as `showInMenu=` under the
    /// configuration's section; default when absent is `true`.
    auto setShowInMenu(const wxString& slug, bool visible) -> bool;

    /// Swap a user-defined configuration with its predecessor in the
    /// display order. Canonical Default is fixed at index 0 and never
    /// participates. No-op for the canonical slug, an unknown slug, or
    /// the first user entry. Returns true when the move actually
    /// happened.
    auto moveUp(const wxString& slug) -> bool;

    /// Mirror of `moveUp` — swap with the next user entry, no-op at the
    /// tail or for canonical.
    auto moveDown(const wxString& slug) -> bool;

private:
    ConfigManager& m_cfg;
    /// Canonical at index 0, user configs follow.
    std::vector<ResolvedCompilerConfig> m_configs;
    /// Resolved value of `compiler.active`, computed once per `reload()`.
    /// Stays `"default"` when the stored slug is missing or unset —
    /// the missing-slug warning is emitted from `reload()` so it fires
    /// at most once per catalog mutation, not on every lookup.
    wxString m_activeSlug { kCanonicalCompilerSlug };
};

} // namespace fbide
