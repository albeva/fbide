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
class ConfigManager;
class Document;
class CompilerConfigCatalog;
struct ResolvedCompilerConfig;

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
 * Root shape depends on the mode:
 *
 * - **Ephemeral**: the single source `File` node *is* the root — there
 *   is no enclosing folder. The file's own path doubles as the
 *   project's on-disk anchor (empty while the buffer is untitled;
 *   set on save). `getRoot()` is null until the file is added.
 * - **Persistent**: a `Folder` root names the directory containing
 *   the project file (empty during in-process creation; set by the
 *   loader) and hosts the file/folder tree underneath.
 *
 * Files (and folders, eventually) are arranged in a flat node arena.
 * `m_nodes` owns each `Node` via `unique_ptr` — keyed by `Node::Id` so
 * the per-node identity round-trips through serialisation — but every
 * intra-tree reference (`Node::parent`, `Folder::children`, `m_root`,
 * the `m_byPath` index, the back-link in `Document::Source`) is a raw
 * `Node*`. The unique_ptr storage guarantees the pointers stay valid
 * for the node's lifetime; IDs are kept for serialisation, debug
 * output, and the occasional temp-file name.
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
        Compile = 1U << 0,
        CompileAndRun = 1U << 1,
        Run = 1U << 2,
        QuickRun = 1U << 3,
    };

    /// Failure modes for the disk-touching tree operations
    /// (`addRealFolder`, `removeNode` with `deleteOnDisk`, `moveNode`
    /// on real-under-real, `renameNode` on real nodes, `setFilePath`
    /// on Persistent). Returned via `std::expected`. `Clash` and
    /// `OutOfTree` are recoverable (caller picks a new name / path);
    /// `IoError` and `InvalidName` are usually fatal for the attempted
    /// op.
    enum class Error : std::uint8_t {
        Clash,       ///< Target path / name already exists on disk.
        IoError,     ///< Filesystem operation failed (permissions, missing parent, etc.).
        InvalidName, ///< newName is empty or contains a path separator.
        OutOfTree,   ///< Target path lives outside the project root (Persistent only).
    };

    /// Opaque strong-typed handle for a `Project` instance. Distinct from
    /// `Node::Id` to prevent accidental mixing at call sites. `0` is the
    /// invalid sentinel; `bool(id)` reports validity.
    using Id = IdentifierBase<Project>;

    /// One entry in the project tree — either a file or a folder. Owned
    /// by `Project::m_nodes` via `unique_ptr`. Intra-tree references
    /// (`parent`, `children`) are non-owning `Node*` whose validity is
    /// pinned by that owning map.
    class Node final {
    public:
        /// Opaque strong-typed handle for a node. UUID-backed so values
        /// round-trip through serialisation; primarily exposed for
        /// persistence and debug output — internal references prefer
        /// the direct `Node*`.
        using Id = IdentifierBase<Node>;

        /// A file node. The `Document*` back-link is populated when the
        /// file is open in a tab; null when the tab is closed (a
        /// persistent project keeps the node around regardless).
        struct File final {
            Document* doc = nullptr;
        };

        /// A folder node. Children are non-owning pointers into the
        /// owning `Project::m_nodes` map. A folder with an empty
        /// `Node::path` is **virtual** — a Visual-Studio-style "filter"
        /// with no on-disk counterpart.
        struct Folder final {
            std::string name;
            std::vector<Node*> children;
        };

        using Entry = std::variant<File, Folder>;

        /// Checks that node holds a Folder
        [[nodiscard]] auto isFolder() const -> bool {
            return std::holds_alternative<Folder>(entry);
        }

        /// If node contains a Folder, get a pointer to it, nullptr otherwise
        [[nodiscard]] auto getFolder() -> Folder* {
            return std::get_if<Folder>(&entry);
        }

        /// Const overload of `getFolder`.
        [[nodiscard]] auto getFolder() const -> const Folder* {
            return std::get_if<Folder>(&entry);
        }

        /// Checks that node holds a File
        [[nodiscard]] auto isFile() const -> bool {
            return std::holds_alternative<File>(entry);
        }

        /// If node contains a File, get a pointer to it, nullptr otherwise
        [[nodiscard]] auto getFile() -> File* {
            return std::get_if<File>(&entry);
        }

        /// Const overload of `getFile`.
        [[nodiscard]] auto getFile() const -> const File* {
            return std::get_if<File>(&entry);
        }

        /// Stable identity (matches the map key in `Project::m_nodes`).
        Id id;
        /// Parent folder, or null for the root.
        Node* parent = nullptr;
        /// On-disk location of this node. Empty for untitled files
        /// (new buffer, never saved) and for virtual folders — matches
        /// `Document::getFilePath()`'s convention so the two layers
        /// stay consistent.
        std::filesystem::path path;
        /// File or Folder
        Entry entry;
    };

    /// Construct an empty project of the given mode. Persistent
    /// projects get a virtual root folder so subsequent `addFile`
    /// calls have somewhere to attach; Ephemeral projects skip that
    /// — their single source `File` *is* the root, populated by the
    /// first (and only) `addFile`. The `ConfigManager` reference is
    /// stored and used by the build-input getters; Persistent projects
    /// (future) will also use it as the fallback for unset overrides.
    Project(ConfigManager& config, Mode mode);

    /// Project identity — unique across the running process.
    [[nodiscard]] auto getId() const -> Id { return m_id; }

    /// Lifetime mode (Ephemeral / Persistent).
    [[nodiscard]] auto getMode() const -> Mode { return m_mode; }

    /// Convenience: true when `getMode() == Mode::Ephemeral`.
    [[nodiscard]] auto isEphemeral() const -> bool { return m_mode == Mode::Ephemeral; }

    /// Insert a file node into the project. For **Ephemeral** projects
    /// the first (and only) `addFile` call *becomes* the root — pass
    /// `parent == nullptr`. For **Persistent** projects `parent` may
    /// be `nullptr` (defaults to `getRoot()`) or any folder owned by
    /// this project. `path` may be empty for an untitled document;
    /// bind it later via `setFilePath`. `doc` is the optional
    /// `Document*` back-link (the project never dereferences it).
    /// @returns The new node, owned by the project; the pointer stays
    /// valid until the node is explicitly removed.
    auto addFile(Document* doc, Node* parent = nullptr) -> Node*;

    /// Insert a virtual folder (Visual-Studio-style "filter" with no
    /// on-disk counterpart). `name` may be any string; collision with
    /// sibling names is allowed (virtual folders are display-only).
    /// `parent == nullptr` defaults to `getRoot()`. Persistent only.
    auto addFolder(Node* parent, std::string name) -> Node*;

    /// Insert a real folder mapped to an on-disk directory. The
    /// directory is created if missing (`fs::create_directories`). The
    /// new folder's `name` mirrors `path.filename()`. `parent ==
    /// nullptr` defaults to `getRoot()`. Persistent only.
    /// @returns the new folder, or `Error::IoError` / `Error::Clash`
    /// when the disk side cannot be set up.
    auto addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error>;

    /// Remove a node from the project. For folders this recursively
    /// removes the whole subtree. Tree mutation is always performed;
    /// when `deleteOnDisk` is true, the node's on-disk artifact (and
    /// its descendants') is also removed via `fs::remove_all`. Disk
    /// failure is reported via the return value but does not roll back
    /// the tree change — once unbound from the project, it stays
    /// unbound.
    /// Preconditions (asserted): `node` is not the root; no file in the
    /// subtree has a bound `Document*` (callers close tabs first).
    auto removeNode(Node* node, bool deleteOnDisk = false) -> std::expected<void, Error>;

    /// Reparent or reorder `node` so it sits at position `index` of
    /// `newParent.children` after the move. Same-parent moves
    /// reorder; different-parent moves reparent. When both `node` and
    /// `newParent` are real (non-empty path), the on-disk artifact is
    /// also moved via `fs::rename` and all descendant paths in the
    /// tree are rewritten to follow.
    /// Preconditions (asserted): `node` is not the root; `newParent`
    /// is a folder; `newParent` is not `node` nor a descendant of
    /// `node`; `index` is within range.
    auto moveNode(Node* node, Node* newParent, std::size_t index) -> std::expected<void, Error>;

    /// Rename `node` in place. Semantics depend on node kind:
    ///   - Virtual folder: update `Folder::name`; always succeeds.
    ///   - Real folder: rename the directory on disk; update the
    ///     folder's path; rewrite every descendant's cached path to
    ///     reflect the new parent prefix; refresh `m_byPath`.
    ///   - File: rename the file on disk (basename portion); update
    ///     `path`; refresh `m_byPath`.
    /// `newName` is the new basename — it must be non-empty and must
    /// not contain a path separator (`/` or `\`).
    auto renameNode(Node* node, std::string newName) -> std::expected<void, Error>;

    /// Resolve an ID back to a live node — useful at the serialisation
    /// boundary or when external references (saved sessions, error
    /// reports referring to a node by ID, …) need to land back on a
    /// concrete pointer. Returns null for an unknown id.
    [[nodiscard]] auto findNode(Node::Id id) -> Node*;

    /// Const overload of `findNode`.
    [[nodiscard]] auto findNode(Node::Id id) const -> const Node*;

    /// Update the path stored on a **file** node and re-key `m_byPath`.
    /// Use after a Save As writes the document to a new path, or when
    /// an untitled file is first saved. Does not touch the file on
    /// disk; the caller has just written it.
    ///
    /// Mode-specific behaviour:
    ///   - **Ephemeral**: always succeeds; the file *is* the root, so
    ///     the new path can sit anywhere — the root moves with it.
    ///   - **Persistent**: rejects with `Error::OutOfTree` when the
    ///     new path doesn't sit under `getRoot()->path` (so projects
    ///     stay self-contained). Always succeeds while the project
    ///     root is empty (loader hasn't set one yet).
    ///
    /// Related ops at a glance:
    ///   - `setFilePath(file, newPath)` — *retarget* a file's identity
    ///     to a path the caller has just produced on disk (Save As /
    ///     first save). No disk side-effect.
    ///   - `renameNode(node, newName)` — *in-place* rename of a file
    ///     or folder; performs the `fs::rename` itself. `newName` is
    ///     a single path component (no separators).
    ///   - `moveNode(node, newParent, index)` — reparent or reorder;
    ///     for real-under-real moves, does the `fs::rename` to follow
    ///     the new tree position.
    auto setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error>;

    /// Drop the `Document*` back-link on the given file node. Used by
    /// `Document::unbindFromProject` so the project-side and document-
    /// side views of "is this doc bound" stay symmetric — without this
    /// call, `getDocuments()` would still report the unbound doc.
    void clearNodeDocument(Node* node);

    /// Root of the project tree. For **Persistent** this is always a
    /// `Folder` representing the project's on-disk anchor directory.
    /// For **Ephemeral** this is the single `File` node — null until
    /// `addFile` is called.
    [[nodiscard]] auto getRoot() -> Node* { return m_root; }
    /// Const overload of `getRoot`.
    [[nodiscard]] auto getRoot() const -> const Node* { return m_root; }

    /// Is `candidate` a path that lives under the project root? Empty
    /// candidate or empty root short-circuits to `true` — used by the
    /// "untitled Ephemeral" and "fresh Persistent" cases where no
    /// constraint can be meaningfully checked yet. Lexical comparison
    /// (no symlink resolution).
    [[nodiscard]] auto isUnderRoot(const std::filesystem::path& candidate) const -> bool;

    /// Snapshot of every currently-bound document in the project. Iteration
    /// order is unspecified — callers that care about a specific order must
    /// sort the result. Unbound file nodes (closed tabs of a persistent
    /// project) are skipped.
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*>;

    /// Get list of sources to be compiled (*.bas) files
    [[nodiscard]] auto getSources() const -> std::vector<Document*>;

    // --- Build / run state ---------------------------------------------
    //
    // `Project` is the sole gateway for build inputs. For `Ephemeral`
    // projects every getter forwards to the injected `ConfigManager` at
    // call time, preserving today's "settings change applies to the
    // next build" behaviour. `Persistent` projects (future) will store
    // their own values; callers don't need to care which mode they're
    // talking to.

    /// Path of the most recently produced build artefact (executable,
    /// library, …) for this project. Empty until the first successful
    /// build. "Artefact" matches the term most build systems use for
    /// build output — `Compile` doesn't always yield an executable.
    [[nodiscard]] auto getArtefact() const -> const std::filesystem::path& { return m_artefact; }

    /// Record the path of the freshly produced build artefact.
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    void setArtefact(std::filesystem::path path) { m_artefact = std::move(path); }

    /// FreeBASIC compile command template (with `<$fbc>` / `<$file>`
    /// meta-tags). Ephemeral: forwards to `compiler.compileCommand` in
    /// `ConfigManager`.
    [[nodiscard]] auto getCompileTemplate() const -> wxString;

    /// Run command template (with `<$file>` / `<$terminal>` / `<$param>`
    /// meta-tags). Ephemeral: forwards to `compiler.runCommand` in
    /// `ConfigManager`.
    [[nodiscard]] auto getRunTemplate() const -> wxString;

    /// Selected compiler-configuration slug — empty means "follow the
    /// active configuration". This is the project's build-config
    /// selection that drives both the toolbar/status-bar dropdown and
    /// the build. Ephemeral: stored on the single bound source document.
    /// Persistent: not implemented yet (will carry its own selection).
    [[nodiscard]] auto getConfigurationSlug() const -> std::optional<wxString>;

    /// Set (or clear) the selected configuration slug. The caller owns
    /// the "matches active → empty" normalisation. Ephemeral: writes
    /// through to the source document. Persistent: not implemented yet.
    void setConfigurationSlug(std::optional<wxString> slug);

    /// Resolve this project's compiler configuration against `catalog`
    /// (the authoritative catalog owned by `CompilerManager`). The
    /// returned reference is owned by the catalog and stays valid until
    /// it reloads. Resolves `getConfigurationSlug()` (empty → the active
    /// configuration). Persistent: not implemented yet.
    [[nodiscard]] auto getCompilerConfig(const CompilerConfigCatalog& catalog) const -> const ResolvedCompilerConfig&;

    /// Entries to populate the build-configuration dropdown with. The
    /// dropdown is driven entirely by the project: Ephemeral projects
    /// pass the catalog's menu-visible compiler configurations through
    /// unchanged (`alwaysInclude` keeps a hidden-but-selected slug
    /// visible). Persistent: not implemented yet — will return the
    /// project's own internally-defined build targets instead.
    [[nodiscard]] auto getMenuConfigurations(const CompilerConfigCatalog& catalog, const wxString& alwaysInclude) const -> std::vector<const ResolvedCompilerConfig*>;

    /// Bitfield of `Capability` values this project supports. Ephemeral
    /// projects unconditionally expose every action (single-file
    /// executable). Persistent projects will compute this from their
    /// stored output kind.
    [[nodiscard]] auto getCapabilities() const -> std::uint8_t;

private:
    /// Internal path mutation — polymorphic over file/folder nodes.
    /// Files re-key `m_pathMap`; folders only update the stored path
    /// (folders don't participate in the file index). Used by
    /// `setFilePath` (the public file-only wrapper), `moveNode`,
    /// `renameNode`, and `rewriteSubtreePaths`.
    void setNodePath(Node* node, const std::filesystem::path& newPath);

    /// Tear down `node` and every descendant: clear path-index entries,
    /// then erase from `m_nodes`. The caller is responsible for first
    /// removing `node` from its parent's children list.
    void destroySubtree(Node* node);

    /// Recursively unlink the on-disk artifacts of `node` and its
    /// descendants. Virtual folders contribute nothing of their own
    /// but their real-pathed descendants are still removed. First
    /// `std::error_code` to fail (if any) is written to `firstErr`;
    /// subsequent failures are ignored so cleanup makes maximum
    /// progress.
    void deleteSubtreeFromDisk(Node* node, std::error_code& firstErr);

    /// Rewrite path-bearing descendants of `folder` so paths that lived
    /// under `oldPrefix` now live under `newPrefix`. Used after a real
    /// folder rename or move. Descendants whose path doesn't sit under
    /// `oldPrefix` are left alone — the project tracks paths
    /// independently, so an unrelated file logically under a folder
    /// can keep its own path.
    void rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix);

    /// Source of build inputs for Ephemeral; fallback for Persistent.
    ConfigManager& m_config;
    /// Project identity (assigned at construction).
    Id m_id;
    /// Project mode
    Mode m_mode;
    /// Project root path
    std::filesystem::path m_path;
    /// Path of the most recently produced build artefact (exe / lib / …).
    std::filesystem::path m_artefact;
    /// Owning storage for every node. unique_ptr pins each node's
    /// address so the cross-tree raw pointers stay valid; the Id key
    /// gives us a stable handle for serialisation round-trips.
    std::unordered_map<Node::Id, std::unique_ptr<Node>> m_nodes;
    /// Path → node lookup index for "does this on-disk path belong to a
    /// project?" queries — e.g. when a user opens a file that's a member
    /// of an open Persistent project, we want to bind the new document
    /// to that project rather than spawn an ephemeral one.
    std::unordered_map<std::filesystem::path, Node*> m_pathMap;
    /// Project tree root (virtual folder for Persistent; single file for Ephemeral).
    Node* m_root = nullptr;
};

/// Underlying-type cast for `Project::Capability` — matches the same
/// `+EnumValue` idiom used elsewhere in the codebase (see `CommandId`).
FBIDE_INLINE constexpr auto operator+(const Project::Capability& cap) -> std::uint8_t {
    return static_cast<std::uint8_t>(cap);
}

} // namespace fbide
