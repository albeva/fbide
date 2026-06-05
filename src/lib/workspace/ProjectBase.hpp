//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "utils/Identifier.hpp"

namespace fbide {
class Document;
class CompilerConfigCatalog;
struct ResolvedCompilerConfig;

/**
 * Abstract interface for a project — the unit the IDE compiles, runs,
 * and exposes in the build-configuration dropdown. Two concrete kinds:
 *
 * - `EphemeralProject` — the single, session-long project that owns every
 *   standalone document (any file not in a persistent project); its build
 *   context follows the active standalone document.
 * - `Project` — persistent, user-created, on-disk; owns a file/folder
 *   tree and (eventually) its own build targets.
 *
 * `ProjectBase` carries only what every project shares: identity, the
 * build artefact, a reference to the single shared compiler-configuration
 * catalog, and the polymorphic build/query interface. Each concrete kind
 * owns its documents (the persistent `Project` via its file nodes, the
 * `EphemeralProject` via a flat list) and outlives them, so a document's
 * `getProject()` back-link is valid for the document's whole lifetime.
 */
class ProjectBase {
public:
    NO_COPY_AND_MOVE(ProjectBase)

    /// Virtual so a derived project can be owned and destroyed through a
    /// `ProjectBase*`.
    virtual ~ProjectBase() = default;

    /// Build / run actions a project supports — bitfield used by
    /// `UIManager::syncBuildCommands` to gate the matching toolbar /
    /// menu commands. `EphemeralProject` is always a single-file
    /// executable (every capability); a persistent `Project` derives it
    /// from its output kind.
    enum class Capability : std::uint8_t {
        Compile = 1U << 0,
        CompileAndRun = 1U << 1,
        Run = 1U << 2,
        QuickRun = 1U << 3,
    };

    /// Opaque strong-typed handle for a project instance, unique across the
    /// running process (a monotonic counter; never serialised — a project is
    /// identified on disk by its file path). `0` is the invalid sentinel;
    /// `bool(id)` reports validity.
    using Id = IdentifierBase<ProjectBase, IdKind::Counter>;

    /// Project identity.
    [[nodiscard]] auto getId() const -> Id { return m_id; }

    /// Display name — shown in the window title bar and as the project-tree
    /// root label. The persistent `Project` returns its user-facing name; the
    /// shared `EphemeralProject` returns an empty string (standalone files
    /// aren't part of a named project).
    [[nodiscard]] virtual auto getName() const -> wxString = 0;

    /// True for `EphemeralProject` (auto-created and torn down with its
    /// single source document); false for the persistent `Project`. Used
    /// by the `WorkspaceManager` lifecycle and `DocumentManager::closeFile`
    /// to decide whether closing the bound document disposes of the project.
    [[nodiscard]] virtual auto isEphemeral() const -> bool { return false; }

    /// Documents currently bound to this project.
    [[nodiscard]] virtual auto getDocuments() const -> std::vector<Document*> = 0;

    /// Documents to compile (the project's `.bas` sources).
    [[nodiscard]] virtual auto getSources() const -> std::vector<Document*> = 0;

    /// Whether `candidate` may live inside this project — backs the
    /// Save-As out-of-tree gate. Ephemeral: always true (the single file
    /// moves freely); persistent: true only under the project root.
    [[nodiscard]] virtual auto isUnderRoot(const std::filesystem::path& candidate) const -> bool = 0;

    // --- Build / run state ------------------------------------------------

    /// Path of the most recently produced build artefact (executable,
    /// library, …). Empty until the first successful build.
    [[nodiscard]] auto getArtefact() const -> const std::filesystem::path& { return m_artefact; }

    /// Selected compiler-configuration slug — empty means "follow the
    /// active configuration". Drives the toolbar/status-bar dropdown and
    /// the build. `EphemeralProject` carries it directly.
    [[nodiscard]] virtual auto getConfigurationSlug() const -> std::optional<wxString> = 0;

    /// Set (or clear) the selected configuration slug. The caller owns
    /// the "matches active → empty" normalisation.
    virtual void setConfigurationSlug(std::optional<wxString> slug) = 0;

    /// Resolve this project's compiler configuration against the injected
    /// catalog (empty slug → the active configuration). The returned
    /// reference is owned by the catalog and stays valid until it reloads.
    [[nodiscard]] auto getCompilerConfig() const -> const ResolvedCompilerConfig&;

    /// Entries to populate the build-configuration dropdown with — driven
    /// entirely by the project. `EphemeralProject` passes the catalog's
    /// menu-visible compiler configurations through (`alwaysInclude` keeps
    /// a hidden-but-selected slug visible); a persistent `Project` will
    /// return its own build targets.
    [[nodiscard]] virtual auto getMenuConfigurations(const wxString& alwaysInclude) const
        -> std::vector<const ResolvedCompilerConfig*> = 0;

    /// Bitfield of `Capability` values this project supports, gating the
    /// build commands.
    [[nodiscard]] virtual auto getCapabilities() const -> std::uint8_t = 0;

protected:
    /// Bound to the single shared compiler-configuration catalog (owned by
    /// `CompilerManager`). Only subclasses construct a project.
    explicit ProjectBase(CompilerConfigCatalog& catalog);

    /// The shared compiler-configuration catalog.
    [[nodiscard]] auto catalog() const -> const CompilerConfigCatalog& { return m_catalog; }

    /// Record the path of the freshly produced build artefact — set only
    /// by the build flow (`BuildTask`).
    void setArtefact(const std::filesystem::path& path) { m_artefact = path; }

    friend class BuildTask;

private:
    /// Project identity (assigned at construction).
    Id m_id;
    /// Path of the most recently produced build artefact (exe / lib / …).
    std::filesystem::path m_artefact;
    /// Single shared compiler-configuration catalog (lifetime: owned by
    /// `CompilerManager`, which outlives all project *usage*).
    CompilerConfigCatalog& m_catalog;
};

/// Underlying-type cast for `ProjectBase::Capability` — matches the same
/// `+EnumValue` idiom used elsewhere in the codebase (see `CommandId`).
FBIDE_INLINE constexpr auto operator+(const ProjectBase::Capability& cap) -> std::uint8_t {
    return static_cast<std::uint8_t>(cap);
}

} // namespace fbide
