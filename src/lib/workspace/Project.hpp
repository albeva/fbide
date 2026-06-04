//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ProjectBase.hpp"
#include "utils/Identifier.hpp"

namespace fbide {
class Document;
class CompilerConfigCatalog;
struct ResolvedCompilerConfig;

/**
 * A persistent, user-created on-disk project: groups one or more source
 * files (and, in the future, other assets) under a folder tree together
 * with the settings needed to build and run them. Survives type changes
 * of its members.
 *
 * Files and folders are arranged in a flat node arena. `m_nodes` owns
 * each `Node` via `unique_ptr` — keyed by `Node::Id` so the per-node
 * identity round-trips through serialisation — but every intra-tree
 * reference (`Node::parent`, `Folder::children`, `m_root`, the
 * `m_pathMap` index, the back-link in `Document::Source`) is a raw
 * `Node*`. The unique_ptr storage guarantees the pointers stay valid for
 * the node's lifetime; IDs are kept for serialisation, debug output, and
 * the occasional temp-file name.
 *
 * **Stub for now.** The node tree (folders, move, rename, under-root
 * constraint) is fully implemented; only the build-configuration surface
 * is stubbed — it reports "nothing configurable" until persistent build
 * targets are implemented. Nothing creates a `Project` at runtime yet;
 * `WorkspaceManager` only spawns `EphemeralProject`.
 */
class Project final : public ProjectBase {
public:
    /// Bound to the single shared compiler-configuration catalog;
    /// construction creates a virtual root folder ready to receive
    /// `addFile` / `addFolder`.
    explicit Project(CompilerConfigCatalog& catalog);

    /// Failure modes for the disk-touching tree operations
    /// (`addRealFolder`, `removeNode` with `deleteOnDisk`, `moveNode` on
    /// real-under-real, `renameNode` on real nodes, `setFilePath`).
    /// Returned via `std::expected`. `Clash` and `OutOfTree` are
    /// recoverable (caller picks a new name / path); `IoError` and
    /// `InvalidName` are usually fatal for the attempted op.
    enum class Error : std::uint8_t {
        Clash,       ///< Target path / name already exists on disk.
        IoError,     ///< Filesystem operation failed (permissions, missing parent, etc.).
        InvalidName, ///< newName is empty or contains a path separator.
        OutOfTree,   ///< Target path lives outside the project root.
    };

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
        /// file is open in a tab; null when the tab is closed (the
        /// project keeps the node around regardless).
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
        /// On-disk location of this node. Empty for untitled files (new
        /// buffer, never saved) and for virtual folders — matches
        /// `Document::getFilePath()`'s convention so the two layers stay
        /// consistent.
        std::filesystem::path path;
        /// File or Folder
        Entry entry;
    };

    // --- Tree management ---------------------------------------------------

    /// Insert a file node under `parent` (`nullptr` defaults to
    /// `getRoot()`). `path` may be empty for an untitled document; bind
    /// it later via `setFilePath`. `doc` is the `Document*` back-link
    /// (never dereferenced). Returns the new node, owned by the project;
    /// the pointer stays valid until the node is explicitly removed.
    auto addFile(Document* doc, Node* parent = nullptr) -> Node*;

    /// Insert a virtual folder (Visual-Studio-style "filter" with no
    /// on-disk counterpart). `name` may be any string; collision with
    /// sibling names is allowed. `parent == nullptr` defaults to `getRoot()`.
    auto addFolder(Node* parent, const std::string& name) -> Node*;

    /// Insert a real folder mapped to an on-disk directory, created if
    /// missing (`fs::create_directories`). The new folder's `name`
    /// mirrors `path.filename()`. `parent == nullptr` defaults to
    /// `getRoot()`. Returns the new folder, or `Error::IoError` /
    /// `Error::Clash` when the disk side cannot be set up.
    auto addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error>;

    /// Remove a node from the project. Folders are removed recursively.
    /// When `deleteOnDisk` is true, the node's on-disk artifact (and its
    /// descendants') is also removed via `fs::remove_all`; disk failure
    /// is reported via the return value but does not roll back the tree
    /// change. Preconditions (asserted): `node` is not the root; no file
    /// in the subtree has a bound `Document*` (callers close tabs first).
    auto removeNode(Node* node, bool deleteOnDisk = false) -> std::expected<void, Error>;

    /// Reparent or reorder `node` to position `index` of
    /// `newParent.children`. When both `node` and `newParent` are real
    /// (non-empty path), the on-disk artifact is moved via `fs::rename`
    /// and all descendant paths in the tree are rewritten to follow.
    /// Preconditions (asserted): `node` is not the root; `newParent` is a
    /// folder, not `node`, and not a descendant of `node`; `index` in range.
    auto moveNode(Node* node, Node* newParent, std::size_t index) -> std::expected<void, Error>;

    /// Rename `node` in place. Virtual folder: update `Folder::name`.
    /// Real folder: rename the directory, update its path, rewrite every
    /// descendant's cached path, refresh `m_pathMap`. File: rename the
    /// file on disk, update `path`, refresh `m_pathMap`. `newName` is the
    /// new basename — non-empty, no path separator (`/` or `\`).
    auto renameNode(Node* node, std::string newName) -> std::expected<void, Error>;

    /// Resolve an ID back to a live node (serialisation boundary,
    /// external references). Returns null for an unknown id.
    [[nodiscard]] auto findNode(Node::Id id) -> Node*;

    /// Const overload of `findNode`.
    [[nodiscard]] auto findNode(Node::Id id) const -> const Node*;

    /// Update the path stored on a **file** node and re-key `m_pathMap`
    /// (after a Save As, or first save of an untitled file). Does not
    /// touch the file on disk; the caller has just written it. Rejects
    /// with `Error::OutOfTree` when `newPath` doesn't sit under the
    /// project root (always succeeds while the root is empty).
    auto setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error>;

    /// Drop the `Document*` back-link on the given file node. Used by
    /// `Document::unbindFromProject` so the project-side and document-side
    /// views of "is this doc bound" stay symmetric.
    void clearNodeDocument(Node* node);

    /// Root folder of the project tree (the on-disk anchor directory).
    [[nodiscard]] auto getRoot() -> Node* { return m_root; }
    /// Const overload of `getRoot`.
    [[nodiscard]] auto getRoot() const -> const Node* { return m_root; }

    // --- ProjectBase interface --------------------------------------------

    /// True when `candidate` sits under the project root (empty candidate
    /// or empty root short-circuits to true). Lexical comparison only.
    [[nodiscard]] auto isUnderRoot(const std::filesystem::path& candidate) const -> bool override;

    /// Every currently-bound document in the tree (unbound file nodes are
    /// skipped). Iteration order unspecified.
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*> override;

    /// Bound `.bas` documents in the tree.
    [[nodiscard]] auto getSources() const -> std::vector<Document*> override;

    // Build configuration — stub (TBD). Persistent projects will carry
    // their own build targets / selection; until then they report
    // "nothing configurable" so the UI shows no dropdown and no build
    // commands for a persistent project.

    [[nodiscard]] auto getConfigurationSlug() const -> std::optional<wxString> override { return std::nullopt; }

    void setConfigurationSlug(std::optional<wxString> /*slug*/) override {}

    [[nodiscard]] auto getMenuConfigurations(const wxString& /*alwaysInclude*/) const
        -> std::vector<const ResolvedCompilerConfig*> override {
        return {};
    }

    [[nodiscard]] auto getCapabilities() const -> std::uint8_t override { return 0; }

private:
    /// Create the virtual root folder and make it the tree root.
    void createVirtualRoot();

    /// Allocate a `File` node for `doc` (with `path`) under `parent`,
    /// register it in the node arena + path index, and bind the document.
    [[nodiscard]] auto attachFileNode(Document* doc, Node* parent, const std::filesystem::path& path) -> Node*;

    /// Internal path mutation — polymorphic over file/folder nodes. Files
    /// re-key `m_pathMap`; folders only update the stored path. Used by
    /// `setFilePath`, `moveNode`, `renameNode`, `rewriteSubtreePaths`.
    void setNodePath(Node* node, const std::filesystem::path& newPath);

    /// Tear down `node` and every descendant: clear path-index entries,
    /// then erase from `m_nodes`. Caller first removes `node` from its
    /// parent's children list.
    void destroySubtree(Node* node);

    /// Recursively unlink the on-disk artifacts of `node` and its
    /// descendants. First `std::error_code` to fail (if any) is written
    /// to `firstErr`; subsequent failures are ignored so cleanup makes
    /// maximum progress.
    void deleteSubtreeFromDisk(Node* node, std::error_code& firstErr);

    /// Rewrite path-bearing descendants of `folder` so paths that lived
    /// under `oldPrefix` now live under `newPrefix`. Descendants outside
    /// `oldPrefix` are left alone.
    void rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix);

    /// Owning storage for every node; the Id key gives a stable handle
    /// for serialisation round-trips.
    std::unordered_map<Node::Id, std::unique_ptr<Node>> m_nodes;
    /// Path → node lookup index for "does this on-disk path belong to a
    /// project?" queries.
    std::unordered_map<std::filesystem::path, Node*> m_pathMap;
    /// Project tree root (virtual folder anchoring the file/folder tree).
    Node* m_root = nullptr;
};

} // namespace fbide
