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
class Context;
class Document;

/**
 * A `Project` groups one or more source files (and, in the future, other
 * assets) together with the settings needed to build and run them.
 *
 * Two flavours exist:
 *
 * - **Ephemeral**: created on the fly for a single standalone FreeBASIC
 *   document — preserves the "new tab → type → run" experience. Lives
 *   only as long as the bound document remains a FreeBASIC source.
 * - **Persistent** *(not implemented in this phase)*: user-created,
 *   on-disk; can group many files and own non-source assets (images,
 *   `Info.plist`, etc.). Survives type changes of its members.
 *
 * Files (and folders, eventually) are arranged in a flat node arena keyed
 * by `Node::Id`; folder children are stored as IDs rather than pointers
 * so re-parenting is O(1), persistence is trivial, and there is exactly
 * one source of truth for the tree shape.
 *
 * `Document*` back-links are stored opaquely — `Project` never
 * dereferences them. Lifetime coupling between a `Project` and its bound
 * documents is enforced by the surrounding `WorkspaceManager` protocol
 * (atomic `Document::bindToProject` / `unbindFromProject` transitions);
 * see @ref project-refactor.
 */
class Project final {
public:
    NO_COPY_AND_MOVE(Project)

    /// Project lifetime mode.
    enum class Mode : std::uint8_t {
        Ephemeral, ///< Created for a single standalone document; lifetime tied to that document.
        Persistent ///< User-created, present on disk; may host many files and non-source assets.
    };

    /// Build / run actions a project supports — bitfield used by
    /// `UIManager::syncBuildCommands` to gate the matching toolbar /
    /// menu commands. A library-output Persistent project, for
    /// example, will return `Compile` only; an executable returns all
    /// four. Ephemeral projects are always single-file executables and
    /// therefore expose every capability.
    enum class Capability : std::uint8_t {
        None          = 0,
        Compile       = 1U << 0,
        CompileAndRun = 1U << 1,
        Run           = 1U << 2,
        QuickRun      = 1U << 3,
    };
    using Capabilities = std::underlying_type_t<Capability>;

    /// Opaque strong-typed handle for a `Project` instance. Distinct from
    /// `Node::Id` to prevent accidental mixing at call sites. `0` is the
    /// invalid sentinel; `bool(id)` reports validity.
    using Id = IdentifierBase<Project>;

    /// One entry in the project tree — either a file or a folder. Stored
    /// in `Project::m_nodes`, indexed by `Node::Id`. Children of a
    /// `Folder` are referenced by ID, not pointer.
    class Node final {
    public:
        /// Opaque strong-typed handle for a node within a single project.
        /// IDs are unique within their owning project but **not** across
        /// projects — never mix IDs between instances.
        using Id = IdentifierBase<Node>;

        /// A file node. The `Document*` back-link is populated when the
        /// file is open in a tab; null when the tab is closed (a
        /// persistent project keeps the node around regardless).
        struct File final {
            Document* doc = nullptr;
        };

        /// A folder node. Children are referenced by ID so re-parenting
        /// and serialisation stay simple. A folder with a null `Node::path`
        /// is **virtual** — a Visual-Studio-style "filter" with no on-disk
        /// counterpart.
        struct Folder final {
            std::string name;
            std::vector<Id> children;
        };

        using Entry = std::variant<File, Folder>;

        Id id;     ///< Self-identifier (matches the map key in `Project::m_nodes`).
        Id parent; ///< Parent folder's ID; invalid for the root.
        /// On-disk location of this node. Empty for untitled files
        /// (new buffer, never saved) and for virtual folders — matches
        /// `Document::getFilePath()`'s convention so the two layers
        /// stay consistent.
        std::filesystem::path path;
        Entry entry;
    };

    /// Construct an empty project of the given mode. Persistent
    /// projects get a virtual root folder so subsequent `addFile`
    /// calls have somewhere to attach; Ephemeral projects skip that
    /// — their single source `File` *is* the root, populated by the
    /// first (and only) `addFile`.
    explicit Project(Mode mode);

    /// Project identity — unique across the running process.
    [[nodiscard]] auto getId() const -> Id { return m_id; }

    /// Lifetime mode (Ephemeral / Persistent).
    [[nodiscard]] auto getMode() const -> Mode { return m_mode; }

    /// Convenience: true when `getMode() == Mode::Ephemeral`.
    [[nodiscard]] auto isEphemeral() const -> bool { return m_mode == Mode::Ephemeral; }

    /// Insert a file node into the project. For **Ephemeral** projects
    /// the new node becomes the root (precondition: no file has been
    /// added yet — Ephemeral projects host exactly one source). For
    /// **Persistent** projects the node is attached as a child of the
    /// existing virtual root folder. `path` may be empty for an
    /// untitled document; bind it later via `setNodePath`. `doc` is
    /// the optional `Document*` back-link (the project never
    /// dereferences it).
    /// @returns The new node's identifier.
    auto addFile(std::filesystem::path path, Document* doc = nullptr) -> Node::Id;

    /// Path stored on the given file or folder node. Returns an empty
    /// path when the node has none (untitled file, virtual folder).
    [[nodiscard]] auto getNodePath(Node::Id id) const -> std::filesystem::path;

    /// Replace the path stored on the given node and re-key `m_byPath`.
    /// Use after a Save As to keep the project's path index in sync with
    /// the document's new on-disk identity.
    void setNodePath(Node::Id id, const std::filesystem::path& path);

    /// The single bound document of an Ephemeral project. Returns nullptr
    /// if no file has been added or no document is bound.
    /// **Defined only for `Mode::Ephemeral`** — asserts otherwise.
    [[nodiscard]] auto getPrimarySource() const -> Document*;

    /// Snapshot of every currently-bound document in the project. Iteration
    /// order is unspecified — callers that care about a specific order must
    /// sort the result. Unbound file nodes (closed tabs of a persistent
    /// project) are skipped.
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*>;

    // --- Build / run state ---------------------------------------------
    //
    // `Project` is the sole gateway for build inputs. For `Ephemeral`
    // projects every getter forwards to `ConfigManager` at call time,
    // preserving today's "settings change applies to the next build"
    // behaviour. `Persistent` projects (future) will store their own
    // values; callers don't need to care which mode they're talking
    // to.

    /// Path of the most recently produced build artefact (executable,
    /// library, …) for this project. Empty until the first successful
    /// build. "Artefact" matches the term most build systems use for
    /// build output — `Compile` doesn't always yield an executable.
    [[nodiscard]] auto getArtefact() const -> const std::filesystem::path& { return m_artefact; }

    /// Record the path of the freshly produced build artefact.
    void setArtefact(std::filesystem::path path) { m_artefact = std::move(path); }

    /// FreeBASIC compile command template (with `<$fbc>` / `<$file>`
    /// meta-tags). Ephemeral: forwards to `compiler.compileCommand` in
    /// `ConfigManager`.
    [[nodiscard]] auto getCompileTemplate(Context& ctx) const -> wxString;

    /// Absolute, resolved path to the FreeBASIC compiler binary (`fbc`).
    /// Ephemeral: forwards to `compiler.path` in `ConfigManager`, resolved
    /// against the IDE's `AppDir`. Returned empty when unset.
    [[nodiscard]] auto getCompilerPath(Context& ctx) const -> wxString;

    /// Run command template (with `<$file>` / `<$terminal>` / `<$param>`
    /// meta-tags). Ephemeral: forwards to `compiler.runCommand` in
    /// `ConfigManager`.
    [[nodiscard]] auto getRunTemplate(Context& ctx) const -> wxString;

    /// Bitfield of `Capability` values this project supports. Ephemeral
    /// projects unconditionally expose every action (single-file
    /// executable). Persistent projects will compute this from their
    /// stored output kind.
    [[nodiscard]] auto getCapabilities() const -> Capabilities;

private:
    /// Allocate a fresh node identifier. Backed by `Uuid::generate()`
    /// so IDs survive serialisation round-trips and stay unambiguous
    /// when project files are merged in version control.
    [[nodiscard]] static auto allocateNodeId() -> Node::Id;

    Id m_id;                                                      ///< Project identity (assigned at construction).
    Mode m_mode;                                                  ///< Ephemeral or Persistent.
    std::filesystem::path m_artefact;                             ///< Path of the most recently produced build artefact (exe / lib / …).
    std::unordered_map<Node::Id, Node> m_nodes;                   ///< Owning storage for every node in the project.
    std::unordered_map<std::filesystem::path, Node::Id> m_byPath; ///< Path → node lookup index (file & folder nodes with a real path).
    Node::Id m_root;                                              ///< The virtual root folder under which top-level entries live.
};

} // namespace fbide
