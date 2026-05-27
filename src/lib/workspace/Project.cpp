//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Project.hpp"
#include "config/ConfigManager.hpp"

using namespace fbide;

Project::Project(ConfigManager& config, const Mode mode)
: m_config(config)
, m_id(Id::generate())
, m_mode(mode) {
    if (mode == Mode::Persistent) {
        // Persistent projects host arbitrary trees; synthesise a virtual
        // root folder so top-level entries have a valid parent. Ephemeral
        // projects skip this — their single `File` becomes the root.
        auto root = std::make_unique<Node>(Node {
            .id = Node::Id::generate(),
            .parent = nullptr,
            .path = {},
            .entry = Node::Folder {
                .name = {},
                .children = {},
            },
        });
        const auto rootId = root->id;
        m_root = root.get();
        m_nodes.emplace(rootId, std::move(root));
    }
}

auto Project::addFile(Node* parent, std::filesystem::path path, Document* doc) -> Node* {
    if (m_mode == Mode::Ephemeral) {
        assert(parent == nullptr && "Ephemeral projects do not take a parent");
        assert(m_root == nullptr && "Ephemeral projects host exactly one file");
    } else {
        assert(parent != nullptr && "Persistent file requires a parent folder");
        assert(std::holds_alternative<Node::Folder>(parent->entry) && "parent must be a folder");
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
    auto* nodePtr = node.get();
    if (!nodePtr->path.empty()) {
        m_byPath.emplace(nodePtr->path, nodePtr);
    }
    m_nodes.emplace(nodeId, std::move(node));

    if (m_mode == Mode::Ephemeral) {
        m_root = nodePtr;
    } else {
        std::get<Node::Folder>(parent->entry).children.push_back(nodePtr);
    }
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

void Project::setFilePath(Node* file, const std::filesystem::path& newPath) {
    assert(file != nullptr);
    assert(std::holds_alternative<Node::File>(file->entry) && "setFilePath is file-only; use renameNode for folders");
    setNodePath(file, newPath);
}

void Project::setNodePath(Node* node, const std::filesystem::path& path) {
    assert(node != nullptr);

    // m_byPath is the *file* lookup index — folders aren't openable so
    // they don't participate. Re-key it only when this is a file node.
    const bool isFile = std::holds_alternative<Node::File>(node->entry);

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
    if (auto* file = std::get_if<Node::File>(&node->entry)) {
        file->doc = nullptr;
    }
}

// --- Tree-management API ---------------------------------------------------

namespace {

// Validate that no file in the subtree carries a Document* back-link.
// removeNode would otherwise leave Document::Source dangling — callers
// must close the relevant tabs through DocumentManager first.
void assertNoBoundDoc(const Project::Node* node) {
    if (const auto* file = std::get_if<Project::Node::File>(&node->entry)) {
        assert(file->doc == nullptr && "removeNode: file has bound document — close tab first");
        return;
    }
    for (const auto* child : std::get<Project::Node::Folder>(node->entry).children) {
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

auto Project::addFolder(Node* parent, std::string name) -> Node* {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    assert(parent != nullptr);
    assert(std::holds_alternative<Node::Folder>(parent->entry) && "parent must be a folder");

    auto node = std::make_unique<Node>(Node {
        .id = Node::Id::generate(),
        .parent = parent,
        .path = {},
        .entry = Node::Folder { .name = std::move(name), .children = {} },
    });
    const auto nodeId = node->id;
    auto* nodePtr = node.get();
    m_nodes.emplace(nodeId, std::move(node));
    std::get<Node::Folder>(parent->entry).children.push_back(nodePtr);
    return nodePtr;
}

auto Project::addRealFolder(Node* parent, std::filesystem::path path) -> std::expected<Node*, Error> {
    assert(m_mode == Mode::Persistent && "folders are Persistent-only");
    assert(parent != nullptr);
    assert(std::holds_alternative<Node::Folder>(parent->entry) && "parent must be a folder");
    assert(!path.empty() && "addRealFolder requires a path");

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
    std::get<Node::Folder>(parent->entry).children.push_back(nodePtr);
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
    // expected return carries the disk failure for the caller to surface.
    std::expected<void, Error> result;
    if (deleteOnDisk && !node->path.empty()) {
        std::error_code ec;
        if (std::holds_alternative<Node::Folder>(node->entry)) {
            std::filesystem::remove_all(node->path, ec);
        } else {
            std::filesystem::remove(node->path, ec);
        }
        if (ec) {
            result = std::unexpected(Error::IoError);
        }
    }

    auto& siblings = std::get<Node::Folder>(node->parent->entry).children;
    std::erase(siblings, node);
    destroySubtree(node);

    return result;
}

auto Project::moveNode(Node* node, Node* newParent, const std::size_t index) -> std::expected<void, Error> {
    assert(node != nullptr);
    assert(node != m_root && "cannot move the project root");
    assert(newParent != nullptr);
    assert(std::holds_alternative<Node::Folder>(newParent->entry) && "newParent must be a folder");

    // Cycle prevention: walking newParent's ancestry must never reach node.
    for (const auto* p = newParent; p != nullptr; p = p->parent) {
        assert(p != node && "moveNode would create a cycle");
    }

    auto& oldChildren = std::get<Node::Folder>(node->parent->entry).children;
    auto& newChildren = std::get<Node::Folder>(newParent->entry).children;
    const bool sameParent = node->parent == newParent;
    // After-move children count for the target — same-parent moves do
    // an erase before the insert, so the upper bound shrinks by one.
    const auto effectiveSize = sameParent ? newChildren.size() - 1 : newChildren.size();
    assert(index <= effectiveSize && "moveNode index out of range");

    // Auto-mv when both endpoints are real and we're actually changing
    // parents. Same-parent reorders don't touch disk. Untitled files
    // and virtual folders also stay put on disk.
    if (!sameParent && !node->path.empty() && !newParent->path.empty()) {
        const auto basename = node->path.filename();
        const auto newPath = newParent->path / basename;
        std::error_code ec;
        if (std::filesystem::exists(newPath, ec)) {
            return std::unexpected(Error::Clash);
        }
        std::filesystem::rename(node->path, newPath, ec);
        if (ec) {
            return std::unexpected(Error::IoError);
        }
        // For a folder, descendants' cached paths need to follow the
        // new prefix; for a file, only the leaf itself.
        if (std::holds_alternative<Node::Folder>(node->entry)) {
            rewriteSubtreePaths(node, node->path, newPath);
        }
        setNodePath(node, newPath);
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
    if (auto* folder = std::get_if<Node::Folder>(&node->entry); folder != nullptr && node->path.empty()) {
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

    if (auto* folder = std::get_if<Node::Folder>(&node->entry)) {
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

void Project::destroySubtree(Node* node) {
    assert(node != nullptr);

    // Tear down children first so any path-index entries they own go
    // away in the right order.
    if (auto* folder = std::get_if<Node::Folder>(&node->entry)) {
        // Snapshot — destroying children would otherwise invalidate
        // the live `children` vector we're iterating.
        const auto children = folder->children;
        for (auto* child : children) {
            destroySubtree(child);
        }
    }
    if (!node->path.empty() && std::holds_alternative<Node::File>(node->entry)) {
        m_byPath.erase(node->path);
    }
    m_nodes.erase(node->id);
}

void Project::rewriteSubtreePaths(Node* folder, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix) {
    assert(folder != nullptr);
    assert(std::holds_alternative<Node::Folder>(folder->entry));

    for (auto* child : std::get<Node::Folder>(folder->entry).children) {
        const bool childIsFolder = std::holds_alternative<Node::Folder>(child->entry);

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

    // Ephemeral: the file IS the root. Empty until `addFile` runs.
    if (m_root == nullptr) {
        return nullptr;
    }
    return std::get<Node::File>(m_root->entry).doc;
}

auto Project::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_nodes.size());
    for (const auto& nodePtr : m_nodes | std::views::values) {
        if (const auto* file = std::get_if<Node::File>(&nodePtr->entry); file != nullptr && file->doc != nullptr) {
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
