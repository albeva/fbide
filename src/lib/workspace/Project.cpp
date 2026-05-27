//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "config/ConfigManager.hpp"

using namespace fbide;

namespace {

// Process cwd, or empty on failure — used as the Ephemeral root path
// fallback when the project is created around an untitled buffer.
auto currentWorkingPath() -> std::filesystem::path {
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    return ec ? std::filesystem::path {} : cwd;
}

// Root folder name derived from the directory's leaf — empty when
// the root has no path.
auto rootFolderName(const std::filesystem::path& path) -> std::string {
    return path.empty() ? std::string {} : path.filename().string();
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
Project::Project(ConfigManager& config, const Mode mode)
: m_config(config)
, m_id(Id::generate())
, m_mode(mode) {
    // Both modes carry a single Folder root that represents the
    // project's on-disk anchor — the directory containing the file
    // (Ephemeral) or the directory containing the project file
    // (Persistent). Ephemeral falls back to cwd while no file has been
    // added; Persistent stays path-less until a loader supplies one.
    std::filesystem::path rootPath;
    if (mode == Mode::Ephemeral) {
        rootPath = currentWorkingPath();
    }
    auto rootName = rootFolderName(rootPath);
    auto root = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = nullptr,
        .path = std::move(rootPath),
        .entry = Node::Folder { .name = std::move(rootName), .children = {} },
    });
    const auto rootId = root->id;
    m_root = m_nodes.emplace(rootId, std::move(root)).first->second.get();
}

// `path` taken by value so callers can move-in; moved into the Node below.
// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto Project::addFile(Node* parent, std::filesystem::path path, Document* doc) -> Node* {
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr && "no parent");
    assert(parent->isFolder() && "parent must be a folder");

    if (m_mode == Mode::Ephemeral) {
        // Ephemeral hosts exactly one file, attached directly under root.
        assert(parent == m_root && "Ephemeral file lives directly under root");
        assert(m_root->getFolder()->children.empty() && "Ephemeral hosts exactly one file");
        // Root folder mirrors the file's containing directory once the
        // file is saved; until then it keeps the cwd from construction.
        if (!path.empty()) {
            m_root->path = path.parent_path();
            m_root->getFolder()->name = rootFolderName(m_root->path);
        }
    } else {
        // Persistent: all members must live under the project root.
        // No-op while root is empty (loader hasn't set one yet).
        assert(isUnderRoot(path) && "Persistent file must live under project root");
    }

    if (!path.empty()) {
        // m_byPath must stay one-key-one-node — a collision would mask
        // one of the entries and silently break "is this path mine?"
        // queries. Trap in debug.
        assert(!m_byPath.contains(path) && "duplicate path in project node index");
    }

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = Node::File { .doc = doc },
    });
    const auto nodeId = node->id;
    auto* nodePtr = m_nodes.emplace(nodeId, std::move(node)).first->second.get();
    if (!nodePtr->path.empty()) {
        m_byPath.emplace(nodePtr->path, nodePtr);
    }
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

auto Project::findNode(const Node::Id id) -> Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findNode(const Node::Id id) const -> const Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void Project::setProjectRoot(std::filesystem::path path) {
    m_root->path = std::move(path);
    m_root->getFolder()->name = rootFolderName(m_root->path);
}

auto Project::isUnderRoot(const std::filesystem::path& candidate) const -> bool {
    // Empty candidate (untitled file) or empty root (unsaved Persistent /
    // Ephemeral pre-init) — no meaningful constraint to enforce.
    if (candidate.empty() || m_root->path.empty()) {
        return true;
    }
    // Lexical comparison only — symlink resolution would require a
    // canonicalisation step that can fail on paths that don't exist yet.
    const auto rel = candidate.lexically_relative(m_root->path);
    return !rel.empty() && !rel.native().starts_with("..");
}

auto Project::setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error> {
    assert(file != nullptr);
    assert(file->isFile() && "setFilePath is file-only; use renameNode for folders");

    // Persistent projects keep all members under the project root —
    // Save As to elsewhere is a UI-layer error. Ephemeral projects
    // skip the check because their root moves to accommodate.
    if (m_mode == Mode::Persistent && !isUnderRoot(newPath)) {
        return std::unexpected(Error::OutOfTree);
    }

    setNodePath(file, newPath);
    if (m_mode == Mode::Ephemeral) {
        // Keep the root folder's path/name aligned with the file's
        // containing directory; if the file becomes untitled, fall
        // back to cwd so the root still names a real directory.
        m_root->path = newPath.empty() ? currentWorkingPath() : newPath.parent_path();
        m_root->getFolder()->name = rootFolderName(m_root->path);
    }
    return {};
}

void Project::setNodePath(Node* node, const std::filesystem::path& path) {
    assert(node != nullptr);

    // m_byPath is the *file* lookup index — folders aren't openable so
    // they don't participate. Re-key it only when this is a file node.
    const bool isFile = node->isFile();

    if (isFile && !node->path.empty()) {
        m_byPath.erase(node->path);
    }
    node->path = path;
    if (isFile && !path.empty()) {
        assert(!m_byPath.contains(path) && "duplicate path in project node index");
        m_byPath.emplace(path, node);
    }
}

void Project::clearNodeDocument(Node* node) {
    assert(node != nullptr);
    if (auto* file = node->getFile()) {
        file->doc = nullptr;
    }
}

// --- Tree-management API ---------------------------------------------------

namespace {

// Validate that no file in the subtree carries a Document* back-link.
// removeNode would otherwise leave Document::Source dangling — callers
// must close the relevant tabs through DocumentManager first.
void assertNoBoundDoc(const Project::Node* node) {
    if (const auto* file = node->getFile()) {
        assert(file->doc == nullptr && "removeNode: file has bound document — close tab first");
        return;
    }
    for (const auto* child : node->getFolder()->children) {
        assertNoBoundDoc(child);
    }
}

// newName must be a single path component — non-empty and free of
// path separators. Reused by rename / future create-folder validation.
auto isValidLeafName(const std::string& name) -> bool {
    return !name.empty()
        && name.find('/') == std::string::npos
        && name.find('\\') == std::string::npos;
}

} // namespace

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto Project::addFolder(Node* parent, std::string name) -> Node* {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr);
    assert(parent->isFolder() && "parent must be a folder");

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = {},
        .entry = Node::Folder { .name = std::move(name), .children = {} },
    });
    const auto nodeId = node->id;
    auto* nodePtr = node.get();
    m_nodes.emplace(nodeId, std::move(node));
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto Project::addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error> {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr);
    assert(parent->isFolder() && "parent must be a folder");
    assert(!path.empty() && "addRealFolder requires a path");
    assert(isUnderRoot(path) && "Persistent folder must live under project root");

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!std::filesystem::is_directory(path, ec)) {
            // Path exists as a non-directory — can't pretend it's a folder.
            return std::unexpected(Error::Clash);
        }
    } else {
        // Create the directory (recursively, so callers don't have to
        // pre-create intermediate parents).
        std::filesystem::create_directories(path, ec);
        if (ec) {
            return std::unexpected(Error::IoError);
        }
    }

    auto name = path.filename().string();
    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = std::move(path),
        .entry = Node::Folder { .name = std::move(name), .children = {} },
    });
    const auto nodeId = node->id;
    auto* nodePtr = node.get();
    m_nodes.emplace(nodeId, std::move(node));
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

auto Project::addFiles(Node* parent, std::span<const std::filesystem::path> paths) -> std::vector<Node*> {
    std::vector<Node*> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
        result.push_back(addFile(parent, path));
    }
    return result;
}

auto Project::removeNode(Node* node, const bool deleteOnDisk) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot remove the project root");
    assertNoBoundDoc(node);

    // Disk delete first (best-effort). Tree mutation runs either way —
    // once the user asks to remove from the project the tree should
    // reach the requested state even if cleanup on disk fails. The
    // expected return carries the first disk failure for the caller to
    // surface.
    std::expected<void, Error> result;
    if (deleteOnDisk) {
        std::error_code firstErr;
        deleteSubtreeFromDisk(node, firstErr);
        if (firstErr) {
            result = std::unexpected(Error::IoError);
        }
    }

    auto& siblings = node->parent->getFolder()->children;
    std::erase(siblings, node);
    destroySubtree(node);

    return result;
}

auto Project::moveNode(Node* node, Node* newParent, const std::size_t index) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot move the project root");
    assert(newParent != nullptr);
    assert(newParent->isFolder() && "newParent must be a folder");

    // Cycle prevention: walking newParent's ancestry must never reach node.
    for (const auto* cursor = newParent; cursor != nullptr; cursor = cursor->parent) {
        assert(cursor != node && "moveNode would create a cycle");
    }

    auto& oldChildren = node->parent->getFolder()->children;
    auto& newChildren = newParent->getFolder()->children;
    const bool sameParent = node->parent == newParent;
    // After-move children count for the target — same-parent moves do
    // an erase before the insert, so the upper bound shrinks by one.
    const auto effectiveSize = sameParent ? newChildren.size() - 1 : newChildren.size();
    assert(index <= effectiveSize && "moveNode index out of range");

    // Auto-mv when we're actually changing parents and `node` has a
    // real on-disk artifact. The destination directory is the closest
    // non-virtual ancestor of `newParent` (walking up through virtual
    // folders, capped at `m_root`). If the walk finds no real
    // ancestor — e.g. an untitled Ephemeral or a Persistent whose
    // loader hasn't set a root path — the disk move is skipped and
    // tree mutation alone runs.
    if (!sameParent && !node->path.empty()) {
        const Node* realParent = newParent;
        while (realParent != nullptr && realParent->path.empty()) {
            realParent = realParent->parent;
        }
        if (realParent != nullptr) {
            const auto basename = node->path.filename();
            const auto newPath = realParent->path / basename;
            std::error_code ec;
            if (std::filesystem::exists(newPath, ec)) {
                return std::unexpected(Error::Clash);
            }
            std::filesystem::rename(node->path, newPath, ec);
            if (ec) {
                return std::unexpected(Error::IoError);
            }
            // For a folder, descendants' cached paths need to follow
            // the new prefix; for a file, only the leaf itself.
            if (node->isFolder()) {
                rewriteSubtreePaths(node, node->path, newPath);
            }
            setNodePath(node, newPath);
        }
    }

    std::erase(oldChildren, node);
    newChildren.insert(newChildren.begin() + static_cast<std::ptrdiff_t>(index), node);
    node->parent = newParent;

    return {};
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto Project::renameNode(Node* node, std::string newName) -> std::expected<void, Error> {
    assert(node != nullptr);

    if (!isValidLeafName(newName)) {
        return std::unexpected(Error::InvalidName);
    }

    // Virtual folder: display-name only, no disk side-effects.
    if (auto* folder = node->getFolder(); folder != nullptr && node->path.empty()) {
        folder->name = std::move(newName);
        return {};
    }

    assert(!node->path.empty() && "real node must have a non-empty path");

    const auto newPath = node->path.parent_path() / newName;
    std::error_code ec;
    if (std::filesystem::exists(newPath, ec)) {
        return std::unexpected(Error::Clash);
    }
    std::filesystem::rename(node->path, newPath, ec);
    if (ec) {
        return std::unexpected(Error::IoError);
    }

    if (auto* folder = node->getFolder()) {
        // Rewrite descendant paths BEFORE updating this folder's own
        // path — rewriteSubtreePaths uses (oldPrefix, newPrefix) to
        // recompute children, and "old" must still match what the
        // descendants currently store.
        rewriteSubtreePaths(node, node->path, newPath);
        folder->name = newName;
    }
    setNodePath(node, newPath);

    return {};
}

void Project::deleteSubtreeFromDisk(Node* node, std::error_code& firstErr) {
    assert(node != nullptr);

    // Recurse into children first — covers virtual folders whose
    // descendants own real paths, and real folders whose descendants
    // sit outside their parent's directory.
    if (auto* folder = node->getFolder()) {
        for (auto* child : folder->children) {
            deleteSubtreeFromDisk(child, firstErr);
        }
    }
    if (node->path.empty()) {
        // Virtual folder or untitled file — nothing of our own on disk.
        return;
    }
    std::error_code ec;
    if (node->isFolder()) {
        // Real folder: remove_all cleans up anything still left
        // (descendants already handled their own tracked paths).
        std::filesystem::remove_all(node->path, ec);
    } else {
        std::filesystem::remove(node->path, ec);
    }
    if (ec && !firstErr) {
        firstErr = ec;
    }
}

void Project::destroySubtree(Node* node) {
    assert(node != nullptr);

    // Tear down children first so any path-index entries they own go
    // away in the right order.
    if (auto* folder = node->getFolder()) {
        // Snapshot — destroying children would otherwise invalidate
        // the live `children` vector we're iterating.
        const auto children = folder->children;
        for (auto* child : children) {
            destroySubtree(child);
        }
    }
    if (!node->path.empty() && node->isFile()) {
        m_byPath.erase(node->path);
    }
    m_nodes.erase(node->id);
}

void Project::rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix) {
    assert(folder != nullptr);
    assert(folder->isFolder());

    for (auto* child : folder->getFolder()->children) {
        const bool childIsFolder = child->isFolder();

        if (child->path.empty()) {
            // Untitled file or virtual folder — nothing to rewrite at
            // this node, but a virtual folder may still own descendants
            // whose paths sit under oldPrefix.
            if (childIsFolder) {
                rewriteSubtreePaths(child, oldPrefix, newPrefix);
            }
            continue;
        }

        std::error_code ec;
        const auto rel = std::filesystem::relative(child->path, oldPrefix, ec);
        if (ec || rel.empty() || rel.native().starts_with("..")) {
            // Child path sits outside oldPrefix — treat as unrelated
            // and don't rewrite this node, but still recurse so
            // descendants under oldPrefix get caught.
            if (childIsFolder) {
                rewriteSubtreePaths(child, oldPrefix, newPrefix);
            }
            continue;
        }

        const auto childNewPath = newPrefix / rel;
        // Recurse first (descendant paths must still match `oldPrefix`
        // at the point they're computed), then update this child.
        if (childIsFolder) {
            rewriteSubtreePaths(child, oldPrefix, newPrefix);
        }
        setNodePath(child, childNewPath);
    }
}

auto Project::getPrimarySource() const -> Document* {
    assert(m_mode == Mode::Ephemeral && "getPrimarySource is ephemeral-only");

    // Ephemeral: root is always present (created in ctor); the single
    // source file is its first (and only) child once addFile has run.
    const auto& children = m_root->getFolder()->children;
    if (children.empty()) {
        return nullptr;
    }
    const auto* file = children.front()->getFile();
    return file != nullptr ? file->doc : nullptr;
}

auto Project::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_nodes.size());
    for (const auto& nodePtr : m_nodes | std::views::values) {
        if (const auto* file = nodePtr->getFile(); file != nullptr && file->doc != nullptr) {
            result.push_back(file->doc);
        }
    }
    return result;
}

// --- Build / run gateway ---------------------------------------------------

auto Project::getCompileTemplate() const -> wxString {
    // Ephemeral projects read through ConfigManager so settings edits
    // take effect immediately on the next build. Persistent projects
    // (future) will return a stored override instead.
    assert(isEphemeral() && "Persistent project compile-options not implemented yet");
    return m_config.config().at("compiler").get_or("compileCommand", R"("<$fbc>" "<$file>")");
}

auto Project::getRunTemplate() const -> wxString {
    assert(isEphemeral() && "Persistent project compile-options not implemented yet");
    return m_config.config().get_or("compiler.runCommand", R"(<$terminal> "<$file>" <$param>)");
}

auto Project::getCapabilities() const -> std::uint8_t {
    // Ephemeral projects host exactly one source file and always
    // produce a runnable executable — every capability applies.
    // Persistent projects (future) will derive this from their stored
    // output kind (Executable / Library / StaticLib / …).
    assert(isEphemeral() && "Persistent project capabilities not implemented yet");
    return +Capability::Compile | +Capability::CompileAndRun | +Capability::Run | +Capability::QuickRun;
}
