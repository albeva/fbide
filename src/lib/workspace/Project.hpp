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
class ConfigManager;
struct ResolvedCompilerConfig;

/**
 * A persistent, user-created on-disk project: a folder tree rooted at a
 * real directory, holding source files and subfolders the IDE builds and
 * runs. Survives type changes of its members.
 *
 * Every node is backed by the filesystem — folders are real directories,
 * files are real files (there are no "virtual" folders). Nodes live in a
 * flat arena: `m_nodes` owns each `Node` via `unique_ptr` keyed by
 * `Node::Id`; every intra-tree reference (`parent`, `children`, `m_root`,
 * the `m_pathMap` index, the back-link in `Document::Source`) is a raw
 * `Node*`, valid for the node's lifetime.
 *
 * This is the **view-model** for `ProjectTreeView`: it owns all tree
 * mutation plus the rules (under-root, name clashes, sort order, which
 * context actions apply). It performs filesystem side-effects (create dir /
 * file, rename, move, trash) but never touches wxWidgets UI — the view
 * drives the dialogs and renders the results.
 *
 * **Build configuration is still stubbed** — persistent build targets land
 * later; until then a project reports "nothing configurable".
 */
class Project final : public ProjectBase {
public:
    /// Bind to the shared compiler-configuration catalog, name the project,
    /// and anchor the tree at `rootDir` (the directory that contains the
    /// `.fbp` file). The root node is created immediately, bound to that
    /// directory.
    Project(CompilerConfigCatalog& catalog, ConfigManager& config, std::string name, std::filesystem::path rootDir);

    /// Out-of-line so the node-owned `unique_ptr<Document>`s see the full
    /// `Document` definition when the tree tears down.
    ~Project() override;

    /// The project's display name — shown as the tree root label, independent
    /// of the root directory's own name. Defaults to the `.fbp` file's stem.
    [[nodiscard]] auto getName() const -> const std::string& { return m_name; }
    /// Set the project's display name (display only; no filesystem effect).
    void setName(std::string name) { m_name = std::move(name); }

    /// Failure modes for the disk-touching tree operations. `Clash` /
    /// `OutOfTree` are recoverable (caller picks a new name / path or offers
    /// to add the existing item); `IoError` / `InvalidName` are usually fatal
    /// for the attempted op.
    enum class Error : std::uint8_t {
        Clash,       ///< Target path / name already exists on disk.
        IoError,     ///< Filesystem operation failed.
        InvalidName, ///< Name is empty or contains a path separator.
        OutOfTree,   ///< Target path lives outside the project root.
        FormatError, ///< `.fbp` could not be parsed (malformed / dangling reference).
    };

    /// How `removeNode` treats the on-disk artifact.
    enum class RemoveMode : std::uint8_t {
        FromProjectOnly, ///< Drop from the tree; leave the file/folder on disk.
        AndTrash,        ///< Also move the file/folder to the OS trash bin.
    };

    /// Context-menu actions a node supports. `Project` decides which apply
    /// (`contextActions`); `ProjectTreeView` renders the labels and runs the
    /// matching handler. Add a shortcut by extending this enum and the
    /// `contextActions` switch.
    enum class Action : std::uint8_t {
        AddFolder,
        AddSourceFile,
        AddHeaderFile,
        AddExisting,
        Remove,
    };

    /// One entry in the project tree — a file or a folder, always backed by a
    /// real filesystem path. Owned by `Project::m_nodes`.
    class Node final {
    public:
        /// Opaque strong-typed handle; a random value rendered as a short
        /// base-62 string so it round-trips through serialisation without
        /// colliding across files. Internal references prefer the direct `Node*`.
        using Id = IdentifierBase<Node, IdKind::Random>;

        /// Ordering applied to a folder's children. `Name` (the default and
        /// currently only option) groups folders before files, then sorts
        /// each group case-insensitively by name. The enum is the extension
        /// point for future orderings.
        enum class SortOrder : std::uint8_t {
            Name,
        };

        /// A file node. Owns its `Document` (created editor-less when the node
        /// is added; the editor is created lazily when the file is opened).
        struct File final {
            std::unique_ptr<Document> doc;
        };

        /// A folder node — a real directory. Children are non-owning pointers
        /// into `Project::m_nodes`, kept ordered per `sort`.
        struct Folder final {
            SortOrder sort = SortOrder::Name;
            std::vector<Node*> children;
        };

        using Entry = std::variant<File, Folder>;

        [[nodiscard]] auto isFolder() const -> bool { return std::holds_alternative<Folder>(entry); }
        [[nodiscard]] auto getFolder() -> Folder* { return std::get_if<Folder>(&entry); }
        [[nodiscard]] auto getFolder() const -> const Folder* { return std::get_if<Folder>(&entry); }
        [[nodiscard]] auto isFile() const -> bool { return std::holds_alternative<File>(entry); }
        [[nodiscard]] auto getFile() -> File* { return std::get_if<File>(&entry); }
        [[nodiscard]] auto getFile() const -> const File* { return std::get_if<File>(&entry); }

        /// The document owned by this file node, or null for a folder.
        [[nodiscard]] auto document() const -> Document* {
            const auto* file = getFile();
            return file != nullptr ? file->doc.get() : nullptr;
        }

        /// Display name — the final path component (folder or file name).
        [[nodiscard]] auto name() const -> std::string { return path.filename().string(); }

        Id id;                      ///< Stable identity (matches the `m_nodes` key).
        Node* parent = nullptr;     ///< Parent folder, or null for the root.
        std::filesystem::path path; ///< Real on-disk location (non-empty for project nodes).
        Entry entry;                ///< File or Folder payload.
    };

    // --- Tree management ---------------------------------------------------

    /// Create a new subfolder `name` under `parent` (null → root): makes the
    /// real directory and adds the node. `Error::Clash` if the directory
    /// already exists (the caller may `addExisting` it instead).
    auto addFolder(Node* parent, const std::string& name) -> std::expected<Node*, Error>;

    /// Create a new empty file `name` under `parent` (null → root): writes an
    /// empty file to disk and adds the node. `name` carries the extension
    /// (the view appends `.bas` / `.bi`). `Error::Clash` if it already exists.
    auto addFile(Node* parent, const std::string& name) -> std::expected<Node*, Error>;

    /// Add an existing on-disk file or folder. `path` must live under the
    /// project root (else `Error::OutOfTree`); missing intermediate folder
    /// nodes between root and `path` are created automatically. A folder is
    /// added as a single node — its contents are not pulled in. Returns the
    /// (possibly pre-existing) leaf node.
    auto addExisting(const std::filesystem::path& path) -> std::expected<Node*, Error>;

    /// Remove `node` (and its subtree) from the project. `AndTrash` also
    /// moves the on-disk artifact to the OS trash. Preconditions (asserted):
    /// not the root; no open document bound in the subtree (caller closes
    /// tabs first).
    auto removeNode(Node* node, RemoveMode mode = RemoveMode::FromProjectOnly) -> std::expected<void, Error>;

    /// Reparent `node` under `newParent` (a folder): renames the artifact on
    /// disk and rewrites descendant paths. `Error::Clash` if the destination
    /// already holds something with that name. Destination stays name-sorted.
    auto moveNode(Node* node, Node* newParent) -> std::expected<void, Error>;

    /// Rename `node` in place (file or folder) on disk and in the tree.
    /// `newName` is the new basename — non-empty, no path separator.
    auto renameNode(Node* node, const std::string& newName) -> std::expected<void, Error>;

    /// Resolve an ID back to a live node, or null.
    [[nodiscard]] auto findNode(Node::Id id) -> Node*;
    /// Const overload of `findNode`.
    [[nodiscard]] auto findNode(Node::Id id) const -> const Node*;

    /// Resolve an on-disk path to its node, or null if the path is not part
    /// of the project.
    [[nodiscard]] auto findByPath(const std::filesystem::path& path) -> Node*;

    /// Update the path stored on a file node and re-key the path index (after
    /// a Save As / first save). `Error::OutOfTree` when `newPath` is not under
    /// the project root.
    auto setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error>;

    /// Root folder of the project tree (the on-disk anchor directory).
    [[nodiscard]] auto getRoot() -> Node* { return m_root; }
    /// Const overload of `getRoot`.
    [[nodiscard]] auto getRoot() const -> const Node* { return m_root; }

    /// Context-menu actions applicable to `node` — folders/root get the Add*
    /// set (root omits Remove); files get Remove.
    [[nodiscard]] auto contextActions(const Node* node) const -> std::vector<Action>;

    // --- Persistence -------------------------------------------------------

    /// Write the project name + folder/file structure to `projectFile` (the
    /// `.fbp`), overwriting it wholesale. Per-user session state is not
    /// touched. `Error::IoError` if the file cannot be written.
    [[nodiscard]] auto saveTo(const std::filesystem::path& projectFile) const -> std::expected<void, Error>;

    /// Construct a project from a `.fbp`: parse the name + structure and
    /// rebuild the tree (preserving node UUIDs), anchored at
    /// `projectFile.parent_path()`. Subsequent mutations auto-save back to
    /// this file. `Error::IoError` when unreadable, `Error::FormatError` when
    /// the contents are malformed.
    [[nodiscard]] static auto loadFrom(
        const std::filesystem::path& projectFile, CompilerConfigCatalog& catalog, ConfigManager& config
    ) -> std::expected<std::unique_ptr<Project>, Error>;

    // --- ProjectBase interface --------------------------------------------

    /// True when `candidate` sits under the project root (empty candidate
    /// short-circuits to true). Lexical comparison only.
    [[nodiscard]] auto isUnderRoot(const std::filesystem::path& candidate) const -> bool override;

    /// Every document in the tree (one per file node).
    [[nodiscard]] auto getDocuments() const -> std::vector<Document*> override;

    /// The `.bas` documents in the tree.
    [[nodiscard]] auto getSources() const -> std::vector<Document*> override;

    // Build configuration — stub (TBD).
    [[nodiscard]] auto getConfigurationSlug() const -> std::optional<wxString> override { return std::nullopt; }

    void setConfigurationSlug(std::optional<wxString> /*slug*/) override {}

    [[nodiscard]] auto getMenuConfigurations(const wxString& /*alwaysInclude*/) const
        -> std::vector<const ResolvedCompilerConfig*> override {
        return {};
    }

    [[nodiscard]] auto getCapabilities() const -> std::uint8_t override { return 0; }

private:
    /// Create the root node bound to `rootDir`.
    void createRoot(std::filesystem::path rootDir);

    /// Create an editor-less `Document` for `path` (type from the extension,
    /// no event sink yet — `DocumentManager` sets the sink when it opens it).
    [[nodiscard]] auto makeDocument(const std::filesystem::path& path) const -> std::unique_ptr<Document>;

    /// Mint a node id that is unique within this project: a random id, re-rolled
    /// on the (astronomically rare) clash with an existing node.
    [[nodiscard]] auto makeNodeId() const -> Node::Id;

    /// Allocate a node under `parent`, register it in the arena + path index,
    /// and append it to the parent's children (caller re-sorts).
    auto attachNode(Node* parent, std::filesystem::path path, Node::Entry entry) -> Node*;
    /// Overload that adopts an explicit `id` — used when loading a `.fbp` so
    /// node UUIDs round-trip. No filesystem side-effects.
    auto attachNode(Node* parent, Node::Id id, std::filesystem::path path, Node::Entry entry) -> Node*;

    /// Find-or-create the folder node for directory `dir` (must exist on disk
    /// under the root), creating intermediate folder nodes as needed.
    auto ensureFolderChain(const std::filesystem::path& dir) -> std::expected<Node*, Error>;

    /// Set a node's path and re-key the path index.
    void setNodePath(Node* node, const std::filesystem::path& newPath);

    /// Sort a folder's children per its `SortOrder`.
    void sortChildren(Node* folder);

    /// Tear down `node` and its subtree (clear path-index entries, erase from
    /// the arena). Caller first unlinks `node` from its parent.
    void destroySubtree(Node* node);

    /// Rewrite descendant paths under `folder` from `oldPrefix` to `newPrefix`
    /// after a rename / move of `folder`.
    void rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix);

    /// Recursively sort `folder` and every descendant folder per its `SortOrder`.
    void sortTree(Node* folder);

    /// Rebuild the tree from a parsed `.fbp` config (folders then files,
    /// preserving UUIDs). `Error::FormatError` on malformed input.
    auto buildFromConfig(wxFileConfig& cfg) -> std::expected<void, Error>;

    /// Persist to `m_projectFile` when set (no-op otherwise). Called after
    /// every successful mutation so the `.fbp` always mirrors the model.
    void autosave() const;

    ConfigManager& m_config;                                     ///< Source of document encoding/EOL/type defaults.
    std::string m_name;                                          ///< Project display name (root label).
    std::filesystem::path m_projectFile;                         ///< `.fbp` path for auto-save (empty = not persisted).
    std::unordered_map<Node::Id, std::unique_ptr<Node>> m_nodes; ///< Owning node arena.
    std::unordered_map<std::filesystem::path, Node*> m_pathMap;  ///< Path → node index (all nodes).
    Node* m_root = nullptr;                                      ///< Tree root (real anchor directory).
};

} // namespace fbide
