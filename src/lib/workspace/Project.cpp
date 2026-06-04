//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "document/Document.hpp"

using namespace fbide;

Project::Project(CompilerConfigCatalog& catalog)
: ProjectBase(catalog) {
    createVirtualRoot();
}

void Project::createVirtualRoot() {
    const auto id = Node::Id::generate();
    auto root = std::make_unique<Node>(Node {
        .id = id,
        .parent = nullptr,
        .path = {},
        .entry = Node::Folder { .name = "", .children = {} },
    });
    m_root = m_nodes.emplace(id, std::move(root)).first->second.get();
}

auto Project::addFile(Document* doc, Node* parent) -> Node* {
    // Persistent tree shape: attach the file under a folder (default
    // the project root). `EphemeralProject` overrides this for its
    // single-file-is-root shape.
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr && parent->isFolder() && "parent must be a folder node");

    auto path = doc->getFilePath();
    if (not path.empty()) {
        if (not isUnderRoot(path)) {
            return nullptr; // REVIEW: should return an error
        }
        if (m_pathMap.contains(path)) {
            return nullptr; // REVIEW: should return an error? existing node?
        }
    }

    auto* ptr = attachFileNode(doc, parent, std::move(path));
    parent->getFolder()->children.push_back(ptr);
    return ptr;
}

auto Project::attachFileNode(Document* doc, Node* parent, const std::filesystem::path& path) -> Node* {
    const auto hasPath = not path.empty();
    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = path,
        .entry = Node::File { .doc = doc },
    });
    auto* ptr = node.get();
    m_nodes.emplace(ptr->id, std::move(node));
    doc->bindToProject(this, ptr);
    if (hasPath) {
        m_pathMap.emplace(ptr->path, ptr);
    }
    return ptr;
}

auto Project::findNode(const Node::Id id) -> Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::findNode(const Node::Id id) const -> const Node* {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second.get() : nullptr;
}

auto Project::isUnderRoot(const std::filesystem::path& candidate) const -> bool {
    // Empty candidate (untitled file) or no anchor yet (Ephemeral pre-first-
    // addFile, fresh Persistent before loader sets the root) — no meaningful
    // constraint to enforce.
    const auto* rootPath = (m_root != nullptr && !m_root->path.empty()) ? &m_root->path : nullptr;
    if (candidate.empty() || rootPath == nullptr) {
        return true;
    }
    // Lexical comparison only — symlink resolution would require a
    // canonicalisation step that can fail on paths that don't exist yet.
    const auto rel = candidate.lexically_relative(*rootPath);
    return !rel.empty() && !rel.native().starts_with("..");
}

auto Project::setFilePath(Node* file, const std::filesystem::path& newPath) -> std::expected<void, Error> {
    assert(file != nullptr);
    assert(file->isFile() && "setFilePath is file-only; use renameNode for folders");

    // Persistent projects keep all members under the project root —
    // Save As to elsewhere is a UI-layer error. `EphemeralProject`
    // overrides this so its root moves to accommodate.
    if (!isUnderRoot(newPath)) {
        return std::unexpected(Error::OutOfTree);
    }

    setNodePath(file, newPath);
    return {};
}

void Project::setNodePath(Node* node, const std::filesystem::path& newPath) {
    assert(node != nullptr);

    // m_pathMap is the *file* lookup index — folders aren't openable so
    // they don't participate. Re-key it only when this is a file node.
    const bool isFile = node->isFile();

    if (isFile && !node->path.empty()) {
        m_pathMap.erase(node->path);
    }

    node->path = newPath;
    if (isFile && !newPath.empty()) {
        assert(!m_pathMap.contains(newPath) && "duplicate path in project node index");
        m_pathMap.emplace(newPath, node);
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

auto Project::addFolder(Node* parent, const std::string& name) -> Node* {
    if (parent == nullptr) {
        parent = m_root;
    }
    assert(parent != nullptr);
    assert(parent->isFolder() && "parent must be a folder");

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = {},
        .entry = Node::Folder { .name = name, .children = {} },
    });
    auto* nodePtr = node.get();
    m_nodes.emplace(node->id, std::move(node));
    parent->getFolder()->children.push_back(nodePtr);
    return nodePtr;
}

auto Project::addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error> {
    assert(!isEphemeral() && "folders are Persistent-only");
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
    if (const auto* folder = node->getFolder()) {
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
    if (const auto* folder = node->getFolder()) {
        // Snapshot — destroying children would otherwise invalidate
        // the live `children` vector we're iterating.
        const auto children = folder->children;
        for (auto* child : children) {
            destroySubtree(child);
        }
    }
    if (!node->path.empty() && node->isFile()) {
        m_pathMap.erase(node->path);
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

auto Project::getSources() const -> std::vector<Document*> {
    // Persistent default: every bound `.bas` document in the tree.
    // `EphemeralProject` overrides this to return its single source.
    std::vector<Document*> result;
    for (const auto& node : m_nodes | std::views::values) {
        if (const auto* file = node->getFile(); file != nullptr && file->doc != nullptr) {
            if (node->path.extension() == ".bas") {
                result.push_back(file->doc);
            }
        }
    }
    return result;
}
