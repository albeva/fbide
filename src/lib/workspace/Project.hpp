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

    /// Opaque strong-typed handle for a `Project` instance. Distinct from
    /// `Node::Id` to prevent accidental mixing at call sites. `0` is the
    /// invalid sentinel; `bool(id)` reports validity.
    class Id final {
    public:
        using Underlying = std::size_t;

        constexpr Id() = default;
        constexpr explicit Id(const Underlying value)
        : m_value(value) {}

        [[nodiscard]] constexpr auto value() const -> Underlying { return m_value; }
        constexpr auto operator<=>(const Id&) const = default;
        explicit constexpr operator bool() const { return m_value != 0; }

    private:
        Underlying m_value = 0;
    };

    /// One entry in the project tree — either a file or a folder. Stored
    /// in `Project::m_nodes`, indexed by `Node::Id`. Children of a
    /// `Folder` are referenced by ID, not pointer.
    class Node final {
    public:
        /// Opaque strong-typed handle for a node within a single project.
        /// IDs are unique within their owning project but **not** across
        /// projects — never mix IDs between instances.
        class Id final {
        public:
            using Underlying = std::size_t;

            constexpr Id() = default;
            constexpr explicit Id(const Underlying value)
            : m_value(value) {}

            [[nodiscard]] constexpr auto value() const -> Underlying { return m_value; }
            constexpr auto operator<=>(const Id&) const = default;
            explicit constexpr operator bool() const { return m_value != 0; }

        private:
            Underlying m_value = 0;
        };

        /// A file node. The on-disk path is optional so untitled documents
        /// (new buffer, never saved) have a valid node before they have a
        /// path; the path is filled in by `Project::setNodePath` on Save As.
        struct File final {
            std::optional<std::filesystem::path> path;
            Document* doc = nullptr;
        };

        /// A folder node. Folders may be **virtual** (`path == nullopt`) —
        /// a Visual-Studio-style "filter" with no on-disk counterpart —
        /// or backed by a real directory. Children are referenced by ID
        /// so re-parenting and serialisation stay simple.
        struct Folder final {
            std::optional<std::filesystem::path> path;
            std::string name;
            std::vector<Id> children;
        };

        using Entry = std::variant<File, Folder>;

        Id id;     ///< Self-identifier (matches the map key in `Project::m_nodes`).
        Id parent; ///< Parent folder's ID; invalid for the root.
        Entry entry;
    };

    /// Construct an empty project of the given mode. A virtual root
    /// folder is created automatically.
    explicit Project(Mode mode);

    /// Project identity — unique across the running process.
    [[nodiscard]] auto getId() const -> Id { return m_id; }

    /// Lifetime mode (Ephemeral / Persistent).
    [[nodiscard]] auto getMode() const -> Mode { return m_mode; }

    /// Convenience: true when `getMode() == Mode::Ephemeral`.
    [[nodiscard]] auto isEphemeral() const -> bool { return m_mode == Mode::Ephemeral; }

    /// Insert a file node under the project root. `path` may be nullopt
    /// for an untitled document; bind it later via `setNodePath`. `doc`
    /// is the optional `Document*` back-link (the project never
    /// dereferences it).
    /// @returns The new node's identifier.
    auto addFile(std::optional<std::filesystem::path> path, Document* doc = nullptr) -> Node::Id;

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

private:
    /// In-class hasher for `Node::Id`. Required because libc++ eagerly
    /// instantiates `std::hash<Node::Id>` to validate the Hash template
    /// argument of `std::unordered_map<Node::Id, …>` at class-definition
    /// time — before the trailing `std::hash` specialisation in this
    /// header is visible. Keeping the hasher in-class side-steps the
    /// instantiation-before-specialisation ordering issue while leaving
    /// the trailing `std::hash` specialisation available for external
    /// callers (e.g. `WorkspaceManager`, tests).
    struct NodeIdHasher final {
        auto operator()(const Node::Id& id) const noexcept -> std::size_t {
            return std::hash<Node::Id::Underlying> {}(id.value());
        }
    };

    /// Allocate a new node identifier. Monotonic per project instance;
    /// values start at 1 (0 is the invalid sentinel).
    auto allocateNodeId() -> Node::Id;

    Id m_id;     ///< Project identity (assigned at construction).
    Mode m_mode; ///< Ephemeral or Persistent.
    std::unordered_map<Node::Id, Node, NodeIdHasher> m_nodes;     ///< Owning storage for every node in the project.
    std::unordered_map<std::filesystem::path, Node::Id> m_byPath; ///< Path → node lookup index (file & folder nodes with a real path).
    Node::Id m_root;                                              ///< The virtual root folder under which top-level entries live.
    Node::Id::Underlying m_nextNodeId = 1;                        ///< Monotonic ID allocator; values start at 1.
};

} // namespace fbide

// Hash specialisations so the opaque ID types can be used as keys in
// `std::unordered_map` / `std::unordered_set`. Defined immediately after
// the public class so any TU including the header gets the specialisation.
namespace std {

template<>
struct hash<fbide::Project::Id> {
    auto operator()(const fbide::Project::Id& id) const noexcept -> size_t {
        return hash<fbide::Project::Id::Underlying> {}(id.value());
    }
};

template<>
struct hash<fbide::Project::Node::Id> {
    auto operator()(const fbide::Project::Node::Id& id) const noexcept -> size_t {
        return hash<fbide::Project::Node::Id::Underlying> {}(id.value());
    }
};

} // namespace std
